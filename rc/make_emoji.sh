#!/bin/bash

curl http://unicode.org/Public/emoji/latest/emoji-test.txt | grep -E '^#|; fully-qualified' \
       | sed -e 's/# group:/group:/' -e 's/# subgroup:/subgroup:/' | egrep -v '^#'  > emoji.tmp

cat emoji.tmp | awk -F "#" '
BEGIN {
	group = "";
	subgroup = "";
}
$0 ~ /^group/ {
	split($1,t,":")
	group = t[2];
	sub(/ /,"",group);
}
$0 ~ /^subgroup/ {
	split($1,t,":")
	subgroup = t[2];
	sub(/ /,"",subgroup);
}
NF==2 {
	sub(/ *; fully-qualified */,"",$1);
	sub(/ /,"",$2);r
       	sub(/ /,"	",$2);
	sub(/flag: /,"",$2);
	sub(/keycap: /,"",$2);
	sub(/E[0-9]+.[0-9]+ /,"",$2);

	split($2,t," ");
	if (group != "") {
		print "";
		print "group	"t[1]"	"group;
		group = "";
	}
	if (subgroup != "") {
		print "";
		print "subgroup	"t[1]"	"subgroup;
		subgroup = "";
	}
	print "emoji	"$2;
}' > emojis
rm emoji.tmp
