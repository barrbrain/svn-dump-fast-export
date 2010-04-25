#!/bin/bash
SVK_DEPOT=""
CO_DIR=svkwc
HASH_DIR=.git/wc

SVN_UUID=`svk pg --revprop -r0 svn:sync-from-uuid /$SVK_DEPOT/`
SVN_URL=`svk pg --revprop -r0 svn:sync-from-url /$SVK_DEPOT/`
MAX_REV=`svk pg --revprop -r0 svn:sync-last-merged-rev /$SVK_DEPOT/`

echo '#' SVN_UUID: $SVN_UUID
echo '#' SVN_URL: $SVN_URL
echo '#' MAX_REV: $MAX_REV

{
  svk co -r1 /$SVK_DEPOT/ $CO_DIR
  cd $CO_DIR
  mkdir -p $HASH_DIR/{0,1,2,3,4,5,6,7,8,9,a,b,c,d,e,f}\
{0,1,2,3,4,5,6,7,8,9,a,b,c,d,e,f}
} 1>&2
for (( REV=1 ; REV<=MAX_REV ; ++REV )) do
  svk up -r$REV | tee .git/svklog 1>&2
  # Extract commit data from SVK
  SVN_AUTHOR=`svk pg --revprop -r$REV svn:author`
  SVN_DATE=`svk pg --revprop -r$REV`

  echo '#' SVN_AUTHOR "$SVN_AUTHOR"
  echo '#' SVN_DATE "$SVN_DATE"

  [ -z "$SVN_AUTHOR" ] && SVN_AUTHOR=nobody

  export GIT_COMMITTER_NAME=$SVN_AUTHOR
  export GIT_COMMITTER_EMAIL=$SVN_AUTHOR"@"$SVN_UUID
  export GIT_COMMITTER_DATE="`date -d "$(echo $(echo $SVN_DATE | tr 'T' ' ' | cut -b1-19) UTC)" '+%s +0000'`"
  export GIT_AUTHOR_NAME="$GIT_COMMITTER_NAME"
  export GIT_AUTHOR_EMAIL="$GIT_COMMITTER_EMAIL"
  export GIT_AUTHOR_DATE="$GIT_COMMITTER_DATE"
  svk pg --revprop -r$REV svn:log > .git/svnmsg
  echo commit refs/heads/master
  echo committer "$GIT_COMMITTER_NAME" "<$GIT_COMMITTER_EMAIL>" "$GIT_COMMITTER_DATE"
  echo data `wc -c < .git/svnmsg`
  cat .git/svnmsg
  echo
  # Process SVK log
  {
    read FIRST_LINE
    while read LOG_LINE ; do
      ACTION="${LOG_LINE:0:1}"
      FILE="${LOG_LINE:4}"
      [ "$ACTION" = D ] && echo D "$FILE" || {
        [ -h "$FILE" ] \
         && LINKFILE=.git/link/"$FILE" \
         && mkdir -p "`dirname "$LINKFILE"`" \
         && readlink -n "$FILE" > "$LINKFILE" \
         && echo "$LINKFILE" >&3
        [ -f "$FILE" ] \
         && echo "$FILE" >&3
      }
    done
  } < .git/svklog 3>.git/modified
  tr '\n' '\0' < .git/modified | xargs -r -0 sha1sum | \
  {
    while read HASH FILE ; do
      LENGTH=`stat -c %s "$FILE"`
      [[ "$FILE" =~ ^\.git ]] && LINK="$FILE" && FILE="${FILE:10}" \
       && MODE=120000 || \
       [ -x "$FILE" ] && MODE=100755 || MODE=100644
      echo M "$MODE" inline "$FILE"
      echo data "$LENGTH"
      [ $MODE = 120000 ] && cat "$LINK" || cat "$FILE"
      echo
      HASH_FILE="$HASH_DIR/${HASH:0:2}/$HASH$MODE"
      ln -f "$HASH_FILE" "$FILE" >/dev/null 2>/dev/null \
       || ln "$FILE" "$HASH_FILE" >/dev/null
    done
  }
  echo
  [[ "$REV" =~ 00$ ]] && find $HASH_DIR -links 1 -exec rm '{}' + 1>&2
done
