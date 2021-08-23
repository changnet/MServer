-- utf8扩展库

--[[
    utf8是Unicode标准(utf8、utf16、utf32)中的一种，由Unicode组织(https://home.unicode.org/)
统一管理标准，并且同步到ISO 10646标准中，但它们有细微的差别(https://www.unicode.org/faq/unicode_iso.html)。
标准文档见：http://www.unicode.org/versions/latest/

以下内容引用自 https://www.unicode.org/versions/Unicode14.0.0/UnicodeStandard-14.0.pdf

UTF-8 encoding form: The Unicode encoding form that assigns each Unicode scalar
value to an unsigned byte sequence of one to four bytes in length, as specified in
Table 3-6 and Table 3-7.
• In UTF-8, the code point sequence <004D, 0430, 4E8C, 10302> is represented
as <4D D0 B0 E4 BA 8C F0 90 8C 82>, where <4D> corresponds to U+004D,
<D0 B0> corresponds to U+0430, <E4 BA 8C> corresponds to U+4E8C, and
<F0 90 8C 82> corresponds to U+10302.
• Any UTF-8 byte sequence that does not match the patterns listed in Table 3-7 is
ill-formed.

Table 3-6 specifies the bit distribution for the UTF-8 encoding form, showing the ranges of
Unicode scalar values corresponding to one-, two-, three-, and four-byte sequences. For a
discussion of the difference in the formulation of UTF-8 in ISO/IEC 10646, see
Appendix C.3, UTF-8 and UTF-16.

          Table 3-6. UTF-8 Bit Distribution
Scalar Value               First Byte   Second Byte   Third Byte   Fourth Byte
00000000 0xxxxxxx          0xxxxxxx
00000yyy yyxxxxxx          110yyyyy     10xxxxxx
zzzzyyyy yyxxxxxx          1110zzzz     10yyyyyy       10xxxxxx
000uuuuu zzzzyyyy yyxxxxxx 11110uuu     10uuzzzz       10yyyyyy     10xxxxxx

1. 这表指定了UTF-8编码在内存中字节分布，xxx、yyy、zzz这些数据都表示这些位会使用到。
2. 除第一位以外，其他位都是以10xxxxxx(10yyyyyy也是表示同样的范围，但不太清楚为啥用不同字母)，
所以第二位开始，(c & 0xC0) == 0x80 都是成立的
3. 第一位计算前面有多少个1，就可以知道该字符占用多少位

Table 3-7 lists all of the byte sequences that are well-formed in UTF-8. A range of byte values such as A0..BF indicates that any byte from A0 to BF (inclusive) is well-formed in that
position. Any byte value outside of the ranges listed is ill-formed. For example:
• The byte sequence <C0 AF> is ill-formed, because C0 is not well-formed in the
“First Byte” column.
• The byte sequence <E0 9F 80> is ill-formed, because in the row where E0 is
well-formed as a first byte, 9F is not well-formed as a second byte.
• The byte sequence <F4 80 83 92> is well-formed, because every byte in that
sequence matches a byte range in a row of the table (the last row).

                    Table 3-7. Well-Formed UTF-8 Byte Sequences
Code Points        First Byte   Second Byte   Third Byte   Fourth Byte
U+0000..U+007F     00..7F
U+0080..U+07FF     C2..DF        80..BF
U+0800..U+0FFF     E0            A0..BF       80..BF
U+1000..U+CFFF     E1..EC        80..BF       80..BF
U+D000..U+D7FF     ED            80..9F       80..BF
U+E000..U+FFFF     EE..EF        80..BF       80..BF
U+10000..U+3FFFF   F0            90..BF       80..BF        80..BF
U+40000..U+FFFFF   F1..F3        80..BF       80..BF        80..BF
U+100000..U+10FFFF F4            80..8F       80..BF        80..BF

In Table 3-7, cases where a trailing byte range is not 80..BF are shown in bold italic to draw
attention to them. These exceptions to the general pattern occur only in the second byte of
a sequence.
As a consequence of the well-formedness conditions specified in Table 3-7, the following
byte values are disallowed in UTF-8: C0–C1, F5–FF.

1. 表3-6指定了字节分布，大概得出了UTF-8的编码范围，但即使在这个范围里，也不一定是合法的UTF-8编码
2. 表3-7指定了UTF-8的具体编码范围，例如，当第一个字节为E0时，则对应表中的第3行，第二个字节
必须在A0..BF范围内，第三个字节必须在80..BF范围，所以<E0 9F 80>不合法
3. 多个字节的字符，最后一个字节范围都是80..BF范围
4. 最小值为0，最大值为10FFFF，表3-7的范围大部分是连续的，只是少了0xD800 .. 0xDFFF这段Surrogate Codes

Code Points
1. Unicode使用一个32位数字来对应一个字符，这个数字通常用U+开头，表示它是一个unicode编码。
当只有一个字节时，unicode兼容ASCII编码，如65都表示字母A。
2. Code Points的算法对应上面的 Table 3-6 中的分布，即一个 Code Points 的值，只能存在
该表中xyzu等位置。该算法我没在标准文件有找到，引用 https://en.wikipedia.org/wiki/UTF-8
  1. The Unicode code point for "€" is U+20AC
  2. 20AC 大小是在U+0800..U+FFFF，所以需要3个字节， 1110zzzz     10yyyyyy       10xxxxxx
  3. 20AC is binary 0010 0000 1010 1100，把这些平均放到zzzzyyyyyyxxxxxx
        第三字节10xxxxxx可以放6bit，即10101100，剩下 0010 0000 10
        第二字节10yyyyyy可以放6bit，即10000010，剩下 0010
        第一字节1110zzzz可以放4bit，即11100010
  4. 最终得到 11100010 10000010 10101100，即 0xE282AC，这个即编码在内存或者文件中的值


https://www.unicode.org/versions/Unicode14.0.0/UnicodeStandard-14.0.pdf
2.4 Code Points and Characters
Note that some abstract characters may be associated with multiple, separately encoded characters (that is, be encoded “twice”). In other
instances, an abstract character may be represented by a sequence of two (or more) other
encoded characters.
同一个字符，有多种表示方式，如 Å（字母A头上有一个圈）可以有 00C5、00212B、0041 030A 三种表示方式
它们的code point不一样，所以在字符串对比也不一样
可以用 print(utf8.char(0xC5), utf8.char(0x212B), utf8.char(0x41, 0x030A), utf8.char(0xC5) == utf8.char(0x41, 0x030A)) 测试
]]

--[[
    Lua中的utf8库，基本上做了utf8的编码、解码，一般情况下够用，如有其他需求，参考
utf8.codes、utf8.char这两函数的逻辑一般也能实现。

解码逻辑在utf8.codes(utf8_decode)
编码逻辑在utfu.char

static const char *utf8_decode (const char *s, utfint *val, int strict) {
  // 对应上面标准标准中 table 3-7 中的code point范围，用于检测是否为合法utf8
  static const utfint limits[] =
        {~(utfint)0, 0x80, 0x800, 0x10000u, 0x200000u, 0x4000000u};
  unsigned int c = (unsigned char)s[0];
  utfint res = 0;  /* final result */
  if (c < 0x80)  /* 单字节编码，相当于ascii */
    res = c;
  else {
    int count = 0;
    // 多字节编码，则可能为 110yyyyy 1110zzzz 11110uuu
    // 0x40 = 0100 0000左移一位就顶掉一个1，用于判断后面接多少字节
    for (; c & 0x40; c <<= 1) {
      unsigned int cc = (unsigned char)s[++count];  /* read next byte */
      // 0xC0 = 1100 0000，0x80 = 1000 0000
      // 第二、三、四字节必须是10uuuuuu的形式，table 3-6 中的规则
      if ((cc & 0xC0) != 0x80)  /* not a continuation byte? */
        return NULL;  /* invalid byte sequence */
      // code point的编码规则，10uuuuuu中只有6位可用，所以左移6位
      // 0x3F是取10uuuuuu中的低6位uuuuuu
      res = (res << 6) | (cc & 0x3F);  /* add lower 6 bits from cont. byte */
    }
    // c & 0x7F是取低位，上面的for循环只循环后面接的字节数，高位仍有一个多余的1
    // 10uuuuuu中只有6位可用，上面的for循环每一位c都左移了一位，所以是 count * (6 - 1)
    res |= ((utfint)(c & 0x7F) << (count * 5));  /* add first byte */

    // 校验code point是否合法
    // count是字符字节数 - 1，根据标准，最大也应该是4 - 1才对，不知道为咐是5
    // 对于limit的值，count < 4时，都是对应 table 3-7， table 3-7给的是一个范围，
    // 如U+0800..U+0FFF，但实际是会出现多个范围，比如第二字节范围是A0..BF，第三字节范
    // 围是 80..BF，算出来就是4个范围，但只要每个字节都符合 (cc & 0xC0) != 0x80
    // 这个规则，那它总的范围就会在table 3-7里的范围
    if (count > 5 || res > MAXUTF || res < limits[count])
      return NULL;  /* invalid byte sequence */
    s += count;  /* skip continuation bytes read */
  }
  if (strict) {
    // 有一个分段称为surrogates codes，通常不当作有效编码
    /* check for invalid code points; too large or surrogates */
    if (res > MAXUNICODE || (0xD800u <= res && res <= 0xDFFFu))
      return NULL;
  }
  if (val) *val = res;
  return s + 1;  /* +1 to include first byte */
}
]]

--[[
    中文与CJK字符集
    在Unicode标准中，中文属于CJK字符集(Chinese, Japanese and Korean)中的一部分，这几
个国家的语言有些编码是相同的，比如常用的 0x4E00..0x9FFF，既在中文使用，也在日韩文中使用

编码分为多个段，每个段都有多种语言会使用这其中的编码
CJK Unified Ideographs 4E00–9FFF Common
CJK Unified Ideographs Extension A 3400–4DBF Rare
CJK Unified Ideographs Extension B 20000–2A6DF Rare, historic
CJK Unified Ideographs Extension C 2A700–2B73F Rare, historic
CJK Unified Ideographs Extension D 2B740–2B81F Uncommon, some in current use
CJK Unified Ideographs Extension E 2B820–2CEAF Rare, historic
CJK Unified Ideographs Extension F 2CEB0–2EBEF Rare, historic
CJK Unified Ideographs Extension G 30000–3134F Rare, historic
CJK Compatibility Ideographs F900–FAFF Duplicates, unifiable variants, corporate characters
CJK Compatibility Ideographs Supplement 2F800–2FA1F Unifiable variants

具体见 https://en.wikipedia.org/wiki/CJK_Unified_Ideographs
]]

-- 校验字符串是否为utf8编码
-- @param s 需要校验的字符串
-- @param i 开始索引，默认为1
-- @param j 结束索引，默认为-1，即字符串长度
-- @return boolean 是否为utf8编码，如果不是，还会返回第一个非法字符的索引
function utf8.valid(s, i, j)
    -- len会检测编码是否有效，无效，则返回nil以及出错的位置
    return utf8.len(s, i, j)
end

-- 校验utf8是否为基础中文
-- @param s 需要校验的字符串
-- @return boolean 是否为中文utf8编码
function utf8.is_base_zh_cn(s)
    -- zh_cn是 ISO 639-1 standard language codes 中的主文简体
    --[[
        https://www.qqxiuzi.cn/zh/hanzi-unicode-bianma.php
        标准在不断更新，下面这个范围不准的，比如很久之前， 9fA6~9FFF之间是空的，但现在
        已经编码到9FFC了，具体到 http://www.unicode.org/charts/PDF/U4E00.pdf 查看

        基本汉字	20902字	4E00-9FA5
        基本汉字补充	74字	9FA6-9FEF
        扩展A	6582字	3400-4DB5
        扩展B	42711字	20000-2A6D6
        扩展C	4149字	2A700-2B734
        扩展D	222字	2B740-2B81D
        扩展E	5762字	2B820-2CEA1
        扩展F	7473字	2CEB0-2EBE0
        扩展G	4939字	30000-3134A
        康熙部首	214字	2F00-2FD5
        部首扩展	115字	2E80-2EF3
        兼容汉字	477字	F900-FAD9
        兼容扩展	542字	2F800-2FA1D
        PUA(GBK)部件	81字	E815-E86F
        部件扩展	452字	E400-E5E8
        PUA增补	207字	E600-E6CF
        汉字笔画	36字	31C0-31E3
        汉字结构	12字	2FF0-2FFB
        汉语注音	43字	3105-312F
        注音扩展	22字	31A0-31BA
        〇	1字	3007
    ]]
    for p, c in utf8.codes(s) do
        if c < 0x4E00 or c > 0x9FFF then return false, p end
    end

    return true
end
