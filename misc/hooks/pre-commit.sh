git diff --cached --name-only | if grep --quiet "docs"
then
  node misc/gendocs/index.js
  git add docs/manual.pdf
fi
