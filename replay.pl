#!/usr/bin/perl

use SVN::Core;
use SVN::Repos;
use SVN::Fs;

use Date::Parse;
use File::Temp;

my $pool = SVN::Pool->new_default;

my $mirror = '/path/to/mirror';

my $repos = SVN::Repos::open($mirror);
my $fs = $repos->fs();

my $uuid = $fs->revision_prop(0, 'svn:sync-from-uuid');
my $url = $fs->revision_prop(0, 'svn:sync-from-url');
my $maxrev = int($fs->revision_prop(0, 'svn:sync-last-merged-rev'));

print "# SVN_UUID: $uuid\n";
print "# SVN_URL: $url\n";
print "# MAX_REV: $maxrev\n";

my $commitlog;
my $nextmark = 1;
my $marks;

$repos->get_logs([''], 1, $maxrev, 1, 0, sub {
  my ($paths, $rev, $author, $date, $log, $pool) = @_;
  my $pool = SVN::Pool->new_default_sub;

  print "# SVN_AUTHOR: $author\n";
  print "# SVN_DATE: $date\n";

  my $GIT_COMMITTER_NAME=$author;
  my $GIT_COMMITTER_EMAIL=$author.'@'.$uuid;
  my $GIT_COMMITTER_DATE=int(str2time($date)).' +0000';

  my @commitlogfile = File::Temp->new();
  $commitlog = $commitlogfile[FH];
  
  print $commitlog "commit refs/heads/master\n";
  print $commitlog "committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE\n";
  print $commitlog 'data '.length($log)."\n$log\n";

  my $root = $fs->revision_root($rev, $pool);

  for my $file (keys %$paths) {
    my $node = $paths->{$file};
    my $action = $node->action;

    if ($action eq 'D') {
      my $path = substr($file, 1);
      print $commitlog "D $path\n";
      next;
    }

    if ($root->is_dir($file, $pool) ) {
      my $path = substr($file, 1);
      print $commitlog "D $path\n";
      modifydir($root, $file, $pool);
    } else {
      modifyfile($root, $file, $pool);
    }
  }

  seek $commitlog, 0, 0;
  local $/ = \16384;
  while (<$commitlog>) {
    print $_;
  }
  close $commitlog;

  print "progress to revision $rev\n"
});

sub modifydir {
  my ($root, $dir, $pool) = @_;
  my $pool = SVN::Pool->new_default_sub;

  my $dirents = $root->dir_entries($dir, $pool);
  for my $name (keys %$dirents) {
    my $entry = $dirents->{$name};
    if ($entry->kind == $SVN::Node::dir) {
      modifydir($root, $dir.'/'.$name, $pool);
    } else {
      modifyfile($root, $dir.'/'.$name, $pool);
    }
  }
}

sub modifyfile {
  my ($root, $file, $pool) = @_;
  my $pool = SVN::Pool->new_default_sub;

  my $proplist = $root->node_proplist($file, $pool);
  my $md5 = $root->file_md5_checksum($file, $pool);
  # Strip the leading slash from the path for git
  my $path = substr($file, 1);

  my $mode;

  if (defined $proplist->{'svn:special'}) {
    $mode = '120000';
    $md5 .= 'L';
  } else {
    $mode = defined $proplist->{'svn:executable'} ? '100755' : '100644';
  }

  my $mark = $marks->{$md5};
  if (!defined $mark) {
    $mark = $nextmark++;
    $marks->{$md5} = $mark;
    my $length = $root->file_length($file, $pool);
    $length -= 5 if $mode == '120000';
    my $contents = $root->file_contents($file, $pool);
    print "# MD5: $md5\n";
    print "blob\nmark :$mark\n";
    print "data $length\n";
    local $/ = \16384;
    my $linkprefix;
    read $contents, $linkprefix, 5 if $mode == '120000';
    while (<$contents>) {
      print $_;
    }
    close $contents;
    print "\n";
  }
  print $commitlog "M $mode :$mark $path\n";
} 
