将数据划分为若干个页，以页作为磁盘和内存之间交互的基本单位，InnoDB中页的大小一般为 16 KB。也就是在一般情况下，一次最少从磁盘中读取16KB的内容到内存中，一次最少把内存中的16KB内容刷新到磁盘中。
行格式，即数据在磁盘上的存放方式，支持以下几种：
1. compact
一条记录包含：额外信息（变长字段长度列表+NULL值列表+记录头信息） + 真实数据（列1~n的数据）

## 聚簇索引
innodb如何生成索引？
每个页中的用户数据都是通过一个链表来存储的，innodb会在链表的表头和尾加入两个自己生成的两个记录infimum和supremum；  
页内的记录根据主键的大小顺序排列成单向链表；  
各个存放‘用户记录’页则按照页中最大的主键值排列成一个双向链表；  
存放‘目录记录’页会有多个不同的层级，同一层级中的页也是按照主键值排列成一个双向链表；  
即：
1. 当‘用户记录’多了，就需要多个页来存储，每个页里面的用户记录是按主键排序的单向链表。但这有一个问题，查询的时候怎么知道‘要查找的记录在哪个页中’呢？
2. 因此需要通过‘目录记录’对多个‘用户记录’的页进行管理，‘目录记录’页中只存储了 主键值 和 页号 这两个信息，也是按照主键值大小排列成单向链表。当一页‘目录记录’不足以管理所有的‘用户记录页’怎么办呢？
3. 那就针对当前这一级别的多个‘目录记录’页再生成上一级的‘目录记录’来管理，更高一级的‘目录记录’页中存储的是下一级‘目录记录’每一页的 [主键值] 和 [页号]。按照这种形式依次类推，最终‘目录记录’和‘用户记录’就会形成一个B+树，非叶子节点是‘目录记录’，叶子节点是‘用户记录’；  
这就是innodb的索引，innodb会为主键自动创建主键索引，也叫聚簇索引；  

## 二级索引
和主键索引类似，innodb也可以根据表中其他的列来建立索引，也会生成一个B+树。与主键索引B+树不一样的是，二级索引存放‘用户记录’的叶子节点只记录了 [列值] 和 [主键]，并根据列值大小排序为单向链表，其中并没有存放‘用户数据’；
同样‘目录记录’中存放的是 [列值] 和 [页号]；  
所以在使用二级索引的时候，实际上是根据 列值 找到其对应的 主键值，再根据 主键值 查找聚簇索引，找到最终的用户记录；  

## 联合索引
使用多个列来进行排序，这个索引在构建的时候‘用户记录’就包含 [列值1] [列值2] ... 和 [主键]，排序的原则是先按[列值1]排序，列值1相同的情况下再按[列值2]排序，依次类推；   
需要注意的是，如果[列值1]都不同，那就不会再按照[列值2]排序了。而‘目录记录’则存放的是[列值1] [列值2] ... 和 [页号]；  
对于 联合索引 有个问题需要注意， ORDER BY 的子句后边的列的顺序也必须按照索引列的顺序给出，如果给出的顺序和联合索引顺序不符合是无法用到索引的。原因就在于联合索引是按照前后顺序来排序的，如果查询的时候想先按[列值2]排序，但实际[列值2]在B+树的存储里是无序的，所以用不上联合索引；  

innodb在实际生产索引（B+树）的时候，最开始是把用户数据都放在根节点。当根节点空间不够的时候，才会生成新的叶子节点来存放‘用户记录’，而根节点则存放‘目录记录’。根节点从一开始的内存地址就是固定的不会变化。也就是说，实际树的生成过程是由根扩展到叶子，而不是人为理解上的由叶子到根；  

## 索引的代价
1. 空间消耗，每创建一个索引就会对应建立一个B+树，索引多数据量大的情况下，即使是‘目录记录’也会消耗掉大量的空间；  
2. 时间消耗，B+树的叶子节点是有序的，那么增删改操作都有可能需要去修改B+树叶子节点的顺序，从而也需要去修改‘目录记录’的顺序。而且索引越多，就需要修改越多的B+树结构；  

## 最左匹配原则
当出现where之后有多个匹配条件的时候，比如where i == 1 and j == 2 and k == 3，那会优先按照i == 1去匹配结果，再匹配j和k；  
建立联合索引的时候，也是按照这个原则来给B+树叶子节点排序的。所以搜索的时候就必须按照索引从左到右的key去搜索，如果先搜索右边的key，实际上右边key可能是无序的是，这时候就会出现全表遍历了；  
还有LIKE语句，如果是`LIKE %aaa%`这种规则，是违反最左匹配原则的，无法使用索引，只能全表扫描；   

## 回表
当我们用二级索引查找全量数据的时候，首先是根据二级索引查找到对应主键值，然后在根据主键值重新查找聚簇索引，才能得到全量数据。这个过程被称为回表；  
回表是有代价的，比如用二级索引查找多个主键值，这个过程是顺序IO，因为二级索引本身是有顺序排列的。但查出来的主键值可能是无序的，那根据主键值再去聚簇索引查询的时候就变成了随机IO这里可能会涉及到多次的磁盘读写，才能得到完整的主键值对应的用户数据；  
如果回表次数过多，实际效果可能还不如直接用聚簇索引去做全表查询。什么情况适合使用回表的方式呢？  
1. 比如limit限制了查询结果的数量，如果数量较少可以使用二级索引+回表的方式；  
如何避免回表带来的性能损耗？查询的时候指定只查询索引列的值，就不需要再回表聚簇索引；  
这种索引本身包含了要查询内容的情况，称之为[覆盖索引]；  

## 如何选择索引
1. 只为用于排序、分组、搜索的列创建索引；  
2. 要考虑列值的离散度，只有离散度高的列创建所以才有意义，如果一个列的列值大部分都是相同的，那创建索引与否都不会对性能有帮助；  
3. 尽量使用数据量小的列作为索引，索引内容少，单页能存放的索引就多，减少磁盘IO；  
4. 如果要对字符串类型设置为索引，那最好是指定前缀长度，尽量不要对整个字符串设置为索引；  
5. 在where语句中不要对索引先计算再比较，这样会出现遍历。比如where id * 2 < 10 和 where id < 10/2，后者是更好的写法（包括函数，也不能对索引做函数运算）；  
6. 对于主键索引，要使用自增值，不要使用类似uuid这种，虽然唯一，但是每次插入的时候都需要重新排序的；  

## 索引失效
1. 查询条件包含or，会导致索引失效；  
2. 隐式类型转换，会导致索引失效，比如字段类型是int，但查询的时候where条件把它当做字符串匹配；  
3. LIKE语句，做中间or尾部字符串匹配时，比如"%abc%"，"%abc"这样会导致索引失效；  
4. 联合索引，查询条件不满足最左匹配原则；  
5. 对索引字段做了算术运算 or 函数运算；  
6. 对索引字段使用!= < > not in，可能导致索引失效；  
7. mysql估计全表扫描比索引更快就会不使用索引；  

## 表的访问方式
1. const   
使用主键 or 唯一二级索引 与 常数值比较，这种访问方式是最快的；  
2. ref  
对于非唯一二级索引 与 常数值比较，然后再通过主键id回表查询；  
注意一下回表，比如where id1 = 1 and id2 > 3；对于这种语句优化器会判断先用哪个二级索引查询扫描行数少，假设先用id1这个二级索引，那么找到符合条件的主键之后，就会先回表；  
在聚簇索引的表里面，找到所有满足主键的表项，再通过遍历来查询id2 > 3的结果。也就说说id2 > 3这个实际上是不会访问二级索引B+树的；  
3. ref or NULL
对于二级索引，不仅要找到某个常数记录，也要找到所有NULL的记录；  
4. range
根据索引进行范围匹配；  
5. index
遍历二级索引的方式为index查询，注意不是遍历聚簇索引；  
6. all
就是全表遍历；  

## 连接
内连接（join、inner join），即同时满足某一个条件的表1和表2的数据交集；  

外连接，分以下几种：
1. 表1 left join 表2，那表1中有表2中没有的取NULL记录，这种做法结果就是A的全量记录；    
2. 表1 left join 表2 where b.id is NULL，这样只输出表1中有表2中没有的记录；  
3. 表1 right join 表2，表2中有而表1中没有的取NULL记录，这种做法结果就是B的全纪录；  
4. 表1 right join 表2 where a.id is NULL，这样只输出表2中有表1中没有的记录；  
5. full join，没有直接语法支持，可以用表1 left join 表2 on 表1.id == 表2.id union 表1 right join 表2 on 表1.id == 表2.id，这样就输出表1和2中全部的记录；   
6. full join 加上 is null，则相当于表1和表2去除掉两者共有，剩余的记录；  