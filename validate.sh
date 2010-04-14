#!/bin/bash
SVK_DEPOT=""
URL="example.com"
CO_DIR=validation
HASH_DIR=hashes
MAX_REV=23000

svk co -r1 /$SVK_DEPOT/ $CO_DIR
mkdir -p $HASH_DIR
for (( REV=1 ; REV<=MAX_REV ; ++REV )) do
  svk up -r$REV $CO_DIR
  # Hashify working copy
  find $CO_DIR -type d -cmin -5 -prune -o \
    -type f -links 1 -exec shasum '{}' + | (
    while read HASH FILE ; do
      [ -x "$FILE" ] && HASH="$HASH"x
      ln "$FILE" $HASH_DIR/$HASH 2>/dev/null || \
        ln -f $HASH_DIR/$HASH "$FILE"
    done
  )
done

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
