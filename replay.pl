#!/usr/bin/perl

use SVN::Core;
use SVN::Repos;
use SVN::Fs;

use Date::Parse;

my $depot = '/path/to/mirror';

my $repos = SVN::Repos::open($depot);
my $fs = $repos->fs();

my $uuid = $fs->revision_prop(0, 'svn:sync-from-uuid');
my $url = $fs->revision_prop(0, 'svn:sync-from-url');
my $maxrev = int($fs->revision_prop(0, 'svn:sync-last-merged-rev'));

print "# SVN_UUID: $uuid\n";
print "# SVN_URL: $url\n";
print "# MAX_REV: $maxrev\n";

$repos->get_logs([''], 1, $maxrev, 1, 0, sub {
  my ($paths, $rev, $author, $date, $log, $pool) = @_;

  print "# SVN_AUTHOR: $author\n";
  print "# SVN_DATE: $date\n";

  my $GIT_COMMITTER_NAME=$author;
  my $GIT_COMMITTER_EMAIL=$author.'@'.$uuid;
  my $GIT_COMMITTER_DATE=int(str2time($date)).' +0000';

  print "commit refs/heads/master\n";
  print "committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE\n";
  print 'data '.length($log)."\n";
  print $log;
  print "\n";

  my $root = $fs->revision_root($rev);

  for my $file (keys %$paths) {
    my $action = $paths->{$file}->action;
    my $mode, $content, $length;

    if ($action eq 'D') {
      print "D $file\n";
      next;
    }

    if ($root->is_dir($file)) {
      next;
    }

    my $proplist = $root->node_proplist($file);

    if (defined $proplist->{'svn:special'}) {
      $mode = '120000';
      $length = $root->file_length($file) - 5;
      my $contents = $root->file_contents($file);
      $contents->READ($content, 5);
      $contents->READ($content, $length);
    } else {
      $mode = defined $proplist->{'svn:executable'} ? '100755' : '100644';
      $length = $root->file_length($file);
      my $contents = $root->file_contents($file);
      $contents->READ($content, $length);
    }

    my $path = substr($file, 1);

    print "M $mode inline $path\n";
    print "data ".$length."\n";
    print $content;
    print "\n";
  } 

  print "progress to revision $rev\n"
});
