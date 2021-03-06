#!/bin/awk -f
# $Id: tr7bit.awk,v 1.4 2005/03/05 21:49:29 zas Exp $

function join(array, start, sep, end, result, i)
{
	if (sep == "")
		sep = " "
	else if (sep == SUBSEP) # magic value
		sep = ""
	result = array[start]
	for (i = start + 1; i <= end; i++)
		result = result sep array[i]
	return result
}

function hex2dec(xx) {
	nn = 0;
	while (xx != "") {
		aa = substr(xx, 1, 1);
		ii = index("0123456789ABCDEF", aa);
		if (!ii) ii = index("0123456789abcdef", aa);
		if (!ii) return -1;
		nn = nn * 16 + ii - 1;
		xx = substr(xx, 2);
	}
	return nn;
}

/^U/{
	gsub("\\\\", "\\\\");
	split(substr($0, 3), z, ":")
	printf("0x%08x:%s\n", hex2dec(z[1]), join(z, 2, ":"));
}
/^0x[0-9a-fA-F]*[	 ]/{
	c = hex2dec(substr($1, 3));
	for (i = 2; i <= NF; i++) {
		if (substr($i, 1, 1) == "#") break;
		if (p = index($i, "-")) {
			p1 = hex2dec(substr($i, 3, p - 3));
			p2 = hex2dec(substr($i, p + 3));
		} else {
			p1 = hex2dec(substr($i, 3));
			p2 = p1;
		}
		for (p = p1; p <= p2; p++) printf("0x%08x:%c\n", p, c);
	}
}
BEGIN{
	printf("0x000000a0:\\001\n0x000000ad:\n");
}
