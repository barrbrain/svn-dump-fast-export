#!/bin/bash
SVK_DEPOT=""
URL="example.com"
CO_DIR=validation
HASH_DIR=hashes
MAX_REV=23000

svk co -r1 /$SVK_DEPOT/ $CO_DIR
( cd $CO_DIR ; git init )
mkdir -p $HASH_DIR/{0,1,2,3,4,5,6,7,8,9,a,b,c,d,e,f}{0,1,2,3,4,5,6,7,8,9,a,b,c,d,e,f}
for (( REV=1 ; REV<=MAX_REV ; ++REV )) do
  svk up -r$REV $CO_DIR
  # Hashify working copy
  ( cd $CO_DIR ; git update-index --refresh )
  ( cd $CO_DIR ; git ls-files -moz |  xargs -0 shasum ) \
    | (
    while read HASH FILE ; do
      FILE="$CO_DIR/$FILE"
      [ -x "$FILE" ] && HASH="$HASH"x
      HASH_FILE="$HASH_DIR/${HASH:0:2}/$HASH"
      ln "$FILE" "$HASH_FILE" 2>/dev/null || \
        ln -f "$HASH_FILE" "$FILE"
    done
  )
  ( cd $CO_DIR ;
    git ls-files -dz | xargs -0 git update-index --info-only --remove 
    git ls-files -moz | xargs -0 git update-index --info-only --add --replace 
  )
done
exit 0
( cd $CO_DIR
  git init
  svk admin dump /$SVK_DEPOT/ | \
    ./svn-dump-fast-export "$URL" | \
    git fast-import --force
  git rev-list master | (
    while read HASH ; do
      REV=`git log -1 --pretty=format:%b $HASH | \
        grep ^git-svn-id | \
        sed 's/^[^@]*@\([0-9]*\) .*$/\1/'`
      [ -z "$REV" ] && continue
      echo $REV $HASH
      svk up -r$REV || continue
      svk revert -R .
      MISMATCH=`git reset $HASH | wc -l`
      [ $MISMATCH -gt 0 ] && echo MISMATCHES: $MISMATCH && break
    done
  ) | tee ../validate.log
)
