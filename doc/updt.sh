#!/bin/sh

fmd() {
cat << EOC
<html>
<head>
  <link rel="stylesheet" type="text/css" href="doc.css">
  <meta charset="UTF-8">
</head>
<body>
$(cat)
</body>
</html>
EOC
}


for x in *.md; do
  t=$(stat -c %Y $x)
  t=$(git log -n 1 --format=%ct $x)
  d=$(LANG= date -d @$t +"%d %B %Y")
  sed -i "s/^\(    Date: *\).*/\1$d/" $x
  markdown -f toc,autolink $x | fmd > ${x%%.md}.html
done

