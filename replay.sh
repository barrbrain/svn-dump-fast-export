#!/bin/bash
SVK_DEPOT=""
CO_DIR=unchanged
HASH_DIR=.git/wc

SVN_UUID=`svk pg --revprop -r0 svn:sync-from-uuid /$SVK_DEPOT/`
SVN_URL=`svk pg --revprop -r0 svn:sync-from-url /$SVK_DEPOT/`
MAX_REV=`svk pg --revprop -r0 svn:sync-last-merged-rev /$SVK_DEPOT/`

echo SVN_UUID: $SVN_UUID
echo SVN_URL: $SVN_URL
echo MAX_REV: $MAX_REV

svk co -r1 /$SVK_DEPOT/ $CO_DIR
cd $CO_DIR
git init
mkdir -p $HASH_DIR/{0,1,2,3,4,5,6,7,8,9,a,b,c,d,e,f}\
{0,1,2,3,4,5,6,7,8,9,a,b,c,d,e,f}
for (( REV=1 ; REV<=MAX_REV ; ++REV )) do
  svk up -r$REV | tee .git/svklog
  # Process SVK log
  grep '^A' .git/svklog | sed 's/^....//' | tr '\n' '\0' \
   | xargs -0 -J % find % -maxdepth 0 -not -type d -print0 > .git/svnadd
  grep '^U' .git/svklog | sed 's/^....//' | tr '\n' '\0' > .git/svnupdate
  grep '^D' .git/svklog | sed 's/^....//' | tr '\n' '\0' > .git/svndelete
  # Add new files
  git update-index -z --add --replace --stdin < .git/svnadd
  # Clear skip-worktree for updated and deleted files
  cat .git/svn{update,delete} | xargs -0 git ls-files -z > .git/filelist
  git update-index -z --no-skip-worktree --stdin < .git/filelist
  # Modify index with updated and removed files
  git update-index -z --remove --stdin < .git/filelist
  # Set skip-worktree for added and updated files
  cat .git/svn{add,update} > .git/filelist
  git update-index -z --skip-worktree --stdin < .git/filelist
  # Hashify working copy
  xargs -0 git ls-files -s < .git/filelist | (
    while read MODE HASH STAGE FILE ; do
      HASH_FILE="$HASH_DIR/${HASH:0:2}/$HASH$MODE"
      ln "$FILE" "$HASH_FILE" 2>/dev/null || \
        ln -f "$HASH_FILE" "$FILE"
    done
  )
  # Extract commit data from SVK
  SVN_AUTHOR=`svk pg --revprop -r$REV svn:author`
  SVN_DATE=`svk pg --revprop -r$REV svn:date`
  export GIT_COMMITTER_NAME=$SVN_AUTHOR
  export GIT_COMMITTER_EMAIL=$SVN_AUTHOR"@"$SVN_UUID
  export GIT_COMMITTER_DATE="`date -juf '%FT%T' $SVN_DATE \
    '+%F %T %z' 2>/dev/null`"
  export GIT_AUTHOR_NAME="$GIT_COMMITTER_NAME"
  export GIT_AUTHOR_EMAIL="$GIT_COMMITTER_EMAIL"
  export GIT_AUTHOR_DATE="$GIT_COMMITTER_DATE"
  # Write tree, commit and advance HEAD
  GIT_PARENT=""
  [ $REV -gt 1 ] && GIT_PARENT="-p `git show-ref --head -s HEAD`"
  GIT_TREE=`git write-tree`
  echo git commit-tree $GIT_TREE $GIT_PARENT
  GIT_COMMIT=`svk pg --revprop -r$REV svn:log \
   | git commit-tree $GIT_TREE $GIT_PARENT`
  git update-ref HEAD $GIT_COMMIT
  git tag $REV
  [[ "$REV" =~ "00$$" ]] && git gc --auto
done
