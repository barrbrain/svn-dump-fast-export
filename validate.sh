#!/bin/bash
SVK_DEPOT=""
URL="example.com"
CO_DIR=validation
HASH_DIR=hashes
MAX_REV=23000

svk co -r1 /$SVK_DEPOT/ $CO_DIR
( cd $CO_DIR ;
  git init
  svk admin dump /$SVK_DEPOT/ | \
    ../svn-fe | \
    git fast-import --force --export-marks=../marks.txt
)
mkdir -p $HASH_DIR/{0,1,2,3,4,5,6,7,8,9,a,b,c,d,e,f}{0,1,2,3,4,5,6,7,8,9,a,b,c,d,e,f}
cat marks.txt | (
for (( REV=1 ; REV<=MAX_REV ; ++REV )) do
  read COMMIT_MARK COMMIT_HASH
  echo $COMMIT_MARK $COMMIT_HASH
  [ $REV -lt 5 ] && continue
  svk up -r$REV $CO_DIR
  # Hashify working copy
  ( cd $CO_DIR ; git update-index --refresh ) >/dev/null
  ( cd $CO_DIR ; git ls-files -moz |  xargs -0 shasum ) \
    2>/dev/null | (
    while read HASH FILE ; do
      FILE="$CO_DIR/$FILE"
      [ -f "$FILE" ] || continue
      [ -x "$FILE" ] && HASH="$HASH"x
      HASH_FILE="$HASH_DIR/${HASH:0:2}/$HASH"
      ln "$FILE" "$HASH_FILE" 2>/dev/null || \
        ln -f "$HASH_FILE" "$FILE"
    done
  )
  ( cd $CO_DIR ; 
    git update-index --refresh >/dev/null
    git ls-files -dz | xargs -0 git update-index --info-only --remove 
    git ls-files -moz | xargs -0 git update-index --info-only --add --replace 
    MISMATCH=`git reset $COMMIT_HASH | wc -l`
    [ $MISMATCH -gt 0 ] && echo MISMATCHES: $MISMATCH && exit 1
    exit 0
  ) || exit 1
done
)
