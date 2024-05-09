# MYSQL基础知识
FROM 《Mysql必知必会》
## 基础操作
1. 选择数据库  
USE database_name;
2. 显示数据库内部表名  
SHOW TABLES;
3. 显示某张表有哪些列  
SHOW COLUMNS FROM table_name;
4. 其他  
SHOW STATUS; 显示服务器状态  
SHOW ERRORS; 显示错误信息
HELP SHOW; 显示允许的show语句

## 数据类型
### char&varchar&text
1、char长度固定， 即每条数据占用等长字节空间；  
2、varchar可变长度，可以设置最大长度；适合用在长度可变的属性；  
3、text不设置长度， 当不知道属性的最大长度时，适合用text；  
按照查询速度： char最快， varchar次之，text最慢；  

1. char（n）和varchar（n）括号中n代表字符的个数，并不代表字节个数，所以当使用了中文的时候(UTF8)意味着可以插入m个中文，但是实际会占用m*3个字节；  
2. 同时char和varchar最大的区别就在于char不管实际value都会占用n个字符的空间，而varchar只会占用实际字符应该占用的空间+1，并且实际空间+1<=n；  
3. 超过char和varchar的n设置后，字符串会被截断。char的上限为255字节，varchar的上限65535字节，text的上限为65535；  
4. char在存储的时候会截断尾部的空格，varchar和text不会。varchar会使用1-3个字节来存储长度，text不会；  

### datetime&timestamp
1. datetime占据8字节，范围'1000-01-01 00:00:00.000000' to '9999-12-31 23:59:59.999999'；  
2. timestamp占据4字节，范围'1970-01-01 00:00:01.000000' to '2038-01-19 03:14:07.999999'；  

## 查询
### SELECT
作用从一个表中检索一个或多个列；  
SELECT 列1,列2,列3 FROM table_name;  
SELECT * FROM table_name;  
#### DISTINCT
表中某一列的数据去重后返回；   
SELECT DISTINCT id FROM table_name;  
如果DISTINCT后面有多个列，则会对所有列都生效，除非多个列去重后结果一样，否则多个列的所有行都会被检索出来；  
### LIMIT
限制输出的内容行数；  
SELECT name FROM table_name LIMIT 5;  
LIMIT默认从是从第一行开始计算内容总数，也支持从指定的某一个行开始计算，比如：  
SELECT name FROM table_name LIMIT 3,4; 表示从第三行开始输出4行内容；  
MYSQL5开始支持一种更为清楚的写法：SELECT name FROM table_name LIMIT 4 OFFSET 3;  

## 排序
### ORDER BY
SELECT id FROM table_name ORDER BY table_name;  
SELECT id,price,name FROM table_name ORDER BY price, name; 先按price排序，price相同的再按name排序；  
#### DESC
指定排序的方式，默认是升序，DESC可以按降序方式排序；  
SELECT id,price,name FROM table_name ORDER BY price DESC, name; 这里是按价格降序排列；  
DESC只能作用于其之前的那一个列，比如上面，price是降序，但name还是升序，如果都想降序，那每个列名之后都要加DESC；   

## 过滤
### WHERE
根据指定的条件，对查询的结果进行过滤；  
SEELCT price FROM table_name WHERE price = 1;  
也支持>、>=、<、<=、!=、BETWEEN 等过滤方式；  
#### BETWEEN
SELECT price FROM table_name WHERE price BETWEEN 5 AND 10;  
#### AND&OR
逻辑操作符添加多个过滤条件；  
SELECT id,price,name FROM table_name WHERE price > 5 AND id = 1000;  
SELECT id,price,name FROM table_name WHERE price > 5 OR id = 1000;  
AND的优先级高于OR；  
#### IN
SELECT id,price,name FROM table_name WHERE id IN (1000, 1001, 1002);  
IN后面还可以接其他SELECT语句；  
#### NOT
SELECT id,price,name FROM table_name WHERE id NOT IN (1000, 1001, 1002);  
NOT还可以与BETWEEN，EXISTS语句进行搭配；  
### LIKE
进行通配符匹配，支持以下几种通配符；  
#### %通配符
%通配符表示任何字符串出现任意次数；  
SELECT id FROM table_name WHERE name LIKE 'abc%'; 查询所有以名字以abc开头的表项的id；  
SELECT id FROM table_name WHERE name LIKE '%abc%'; 查询所有名字中包含abc的表项的id；  
SELECT id FROM table_name WHERE name LIKE 'a%c'; 查询所有名字以a开头以c结尾的表项的id；  
#### _通配符
下划线通配符作用和%通配符类似，但只能匹配单个字符，不能匹配多个字符；   
SELECT id FROM table_name WHERE name LIKE 'a_'; 可以查询到name是ab、ac这类，但查不到abc；  

## 字段处理
### 拼接Concat()
SELECT Concat(name,'(',country,')') FROM table_name ORDER BY name; 最终输出结果是类似abc (chn)这样的拼接字段，以name进行排序；  
但这里有一个问题即输出内容缺少一个key来对应，应用通过该语句查询的时候，可能无法使用这个结果。因此需要给生成的拼接字段一个别名，如：  
SELECT Concat(name,'(',country,')') AS title FROM table_name ORDER BY name;  
### 算术运算（+ - * /）
SELECT id, price, num, price*num AS total_value FROM table_name ORDER BY id;   
### 文本处理函数
1. Length()，返回字符串的长度；  
2. Upper()，将字符串都改为大写字母；  
3. Lower()，将字符串都改为小写字母；  
4. Ltrim()、Rtrim()，去掉字符串左右的空格；  
### 时间处理函数
### 数值处理函数
### 聚集函数
1. AVG()，返回某列的平均值；  
2. COUNT()，返回某列的值的个数；  
3. MAX()、MIN()，返回某列的最大、最小值；  
4. SUM()，返回某列值之和；  
单条SELECT语句中可以包含多个聚集函数；  
### 分组
#### GROUP BY
SELECT id, COUNT(*) AS id_nums FROM table_name GROUP BY id;  按id进行分组，返回每个id对应的数据个数；  
GROUP BY必须出现在WHERE之后，ORDER BY之前；  
如果使用了聚集函数，则必须使用GROUP BY；  
#### HAVING
HAVING类似于WHERE，但WHERE只能过滤列，HAVING则用于过滤分组；  

### 子查询

正则表达式


innodb的记录是按照行格式，有四种不同的行格式。每一行都分为真实数据信息 + 其他额外信息 + next record指针。四种行格式的区别实在真实数据信息部分；  
页中的数据记录是按照主键从小到大通过单向链表排序的。
如果不设置主键会如何？innodb会选择一个unique键作为主键；如果连unique主键都没有，那innodb就会生产一个row unique id的隐藏键作为主键；  
所以一般是用自增id作为主键（主键索引是一定存在的），这样每次插入新的记录时，只需要将新纪录插入到链表尾部即可。而使用其他的主键，插入会有比较大的性能开销；  

数据记录要怎么分组？
infmum单独一个组；
supremum记录在的分组记录条数1~8条；
其余分组记录条数4~8条；

innodb一个页大小是16KB，即每次都是按16KB的大小向磁盘写入或从磁盘读取数据；  
innodb每一页的字段都是按照主键串联成单向链表，单向链表的表头和表尾是innodb自己生成的两个记录infimum和supremum；  
将每个页中的数据分成若干组，选择每个组最大的一个记录作为组长，组长记录组员的个数，然后将组长地址加入到槽中，槽是一段连续的物理空间。通过二分法查找槽就可以快速找到某个记录；   
多个数据页之间是通过双向链表串联。那当要查询一个记录时，如何知道其所处的页号呢？
innodb的做法是将多个表的信息也写入到一个数据页中，其结构和普通的数据页一样，只是页中的每一项记录不是用户数据而是表信息，称之为目录项记录。每一个目录项记录都包含了该数据页的最小主键值和页号。这样查询的时候先通过二分法找到数据对应的页号，再到对应的页查询数据；   
如果数据页数量多到一个目录项记录页装不下怎么办？
方法还是一样，即为多个目录项记录页再创建一个目录（一个新的数据页）。
这一系列的关联关系就是索引，其组织形式就是B+树，mysql根据主键创建的索引就是主键索引，也叫聚簇索引；  
总结主键索引的特点：
1. 按照主键的大小对用户记录和数据页进行排序，记录使用单向链表、数据页使用双向链表来管理；  
2. B+树的叶子节点保存了用户的完整记录；  

到此根据主键索引查询速度快的原因已经明确了。但实际使用时，查询操作不一定都是根据主键查的，如果要根据其他字段查询怎么办？
可以将经常会用来查询的字段设置为普通索引，也叫二级索引。普通索引也会构成一颗B+数，但这个B+树的叶子节点并不像主键索引保存了完整的用户记录。而是只保存了普通索引对应数据的主键索引；同时这颗B+树节点中的记录不再是按照主键索排序，而是按照普通索引排序的单向链表；  
即通过普通索引快速查找到主键索引，再通过主键索引找到最终的用户数据；  


B树和B+树有什么区别，为什么要使用B+树？
B树是多叉树，允许每个节点有超过2个的叶子节点。传统二叉树在大量数据的情况下无法做到磁盘读取的友好型，即逻辑上两个相近的叶子节点在实际存储的时候可能存在相隔很远的内存地址中。无法有效利用磁盘预读（局部性原理）的能力；  
B树一个节点里可以存放多个数据，这样有效的降低了树的高度，同时能够保证节点内存的连续性；  
B+树则是在B树的基础上把数据全部存储在叶子节点中，非叶子节点只存储key，并没有真正的数据。同时所有的叶子节点通过链表进行串联；  
区别：  
1. 由于B+的非叶子节点不存储数据data，所以B+树节点的大小不一样。而B树的节点大小都一样，等于内存页大小；  
2. B+树数据都在叶子节点，所以查询时间复杂度固定为logn，而B树查询时间复杂度不固定，最好为O(1)；  
3. B+树能更好的利用局部性原理，因为所有的数据都在叶子节点，当访问某一个数据时，其相邻数据也会被加载到内存，可以减少IO次数；  
4. 由于B+树非叶子节点不存储数据data，因此相同空间可以存放更多的索引，也就意味着单次磁盘IO能够读取到更多的索引值，效率更高；  

大量数据下count(*)速度慢的优化
count (字段）：遍历整张表，需要取值，判断 字段！= null，按行累加；
count (主键) ：遍历整张表，需要取 ID，判断 id !=null，按行累加；
count (1) ：遍历整张表，不取值，返回的每一行放一个数字 1，按行累加；
count (*)：不会把全部字段取出，专门做了优化，不取值。count ( * ) 肯定不是 null，按行累加。
问题原因是使用主键查询，主键B+树保存了用户数据，查询过程会导致全量数据被缓存到内存里，磁盘IO会消耗大量的时间；  
优化方式就是使用二级索引，二级索引的B+树只会存储主键信息，数据量大大减小；  
但如果数据量大打一定程度，二级索引的数据量也会很大，此时就需要一些别的办法：  
1. 如果准确性要求不高，可以在应用层缓存一个计数值，通过定时器或者其他方式低频的从数据库同步；  
2. 对于准确性高的，就需要通过在mysql里面增加一个计数表，配合事务机制来保证计数的准确性；  