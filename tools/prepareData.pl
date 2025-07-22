#!/usr/bin/perl

use strict;

use POSIX qw(strftime);
use File::Path 'remove_tree';
use File::Copy;
use Cwd;
use CSS::Minifier qw(minify);
use JavaScript::Minifier qw(minify);
# use HTML::Packer;

my $srcPath__ = '../fssrc';
my $dataPath__ = '../data';
my $templatesParh__ = '../templates';
my $jsonFile = "$dataPath__/fsdata.js";
my $esp32IP = '192.168.227.153';
my $dontMinifyCSSmin = 'NO'; # 'YES' or 'NO' - do not minify compressed CSS
my $autoUpload2LittleFS = 'YES';
my $tmpPath__ = '../tmp';
my $gzipMinSize = 255;
my $gzipDiasable = 0;

my $json__;
my $uploadfs = 0;
my $notBuildNumUp = 0;

foreach my $arg (@ARGV) {
    if($arg eq '--uploadfs') {
        $uploadfs = 1;
    }
    if($arg eq '--not_build_num_up') {
        $notBuildNumUp = 1;
    }
    if($arg eq '--gzip_disable') {
        $gzipDiasable = 1;
    }
}

unless (-d $dataPath__) {
    mkdir($dataPath__) or die "Can't create folder '$dataPath__': $!";
    print "$dataPath__\t[Created]\n";
} else {
    print "$dataPath__\t[Already exists]\n";
}

unless (-d $tmpPath__) {
    mkdir($tmpPath__) or die "Can't create folder '$tmpPath__': $!";
    print "$tmpPath__\t[Created]\n";
} else {
    print "$tmpPath__\t[Already exists]\n";
}

# Update /assets/js/tools.js
if(!$notBuildNumUp) {
    my $toolsJsPath = $srcPath__ . '/assets/js/tools.js';
    copy($toolsJsPath, $tmpPath__ . '/tools.js') or die "Can't copy file '$toolsJsPath' -> ' $tmpPath__/tools.js': $!";
    my $toolsJs = '';
    open(TJS, '<' . $toolsJsPath) or die "Can't open file '$toolsJsPath': $!\n";

    while(my $line = <TJS>) {
        if($line =~ /var __BUILD__ = '(.+?)';/) {
            my $build = $1;
            $build += 1;
            $line = 'var __BUILD__ = \'' . sprintf("%05d", $build) . "';\n"
        }
        $toolsJs .= $line;
    }
    close(TJS);
    open(TJS, '>' . $toolsJsPath) or die "Can't open file '$toolsJsPath': $!\n";
    print TJS $toolsJs;
    close(TJS);
    print "$toolsJsPath\t[Updated]\n";
}

# Update /assets/js/load_assets.js
my $lJsPath = $srcPath__ . '/assets/js/load_assets.js';
copy($lJsPath, $tmpPath__ . '/load_assets.js') or die "Can't copy file '$lJsPath' -> ' $tmpPath__/load_assets.js': $!";
my $load_assetsJS = '';
open(TJS, '<' . $lJsPath) or die "Can't open file '$lJsPath': $!\n";

while(my $line = <TJS>) {
    if ($line =~ /{ url: '\/assets\/js\/tools\.js\?v=([^']+)' },/) {
        my $build = $1;
        $build += 1;
        $line = '  { url: \'/assets/js/tools.js?v=' . sprintf("%d", $build) . "' },\n";
    }
    $load_assetsJS .= $line;
}
close(TJS);
open(TJS, '>' . $lJsPath) or die "Can't open file '$load_assetsJS': $!\n";
print TJS $load_assetsJS;
close(TJS);
print "$lJsPath\t[Updated]\n";

opendir(my $dh, $dataPath__) or die "Can't open $dataPath__: $!";
while (my $entry = readdir($dh)) {
    next if $entry eq '.' or $entry eq '..';
    my $path = "$dataPath__/$entry";
    print "Remove $path...\n";
    remove_tree($path);
    print "$path\t[DONE]\n";
}
closedir($dh);

goProcess($srcPath__);
my $mtime = time();
my $jsonLine;
if($gzipDiasable) {
    $jsonLine = "{\"path\":\"/fsdata.js\",\"mtime\":\"$mtime\"}";
} else {
    $jsonLine = "{\"path\":\"/fsdata.js.gz\",\"mtime\":\"$mtime\"}";
}
push @$json__, $jsonLine;
my $json = "var fsData = [\n" . join(",\n", @$json__) . "\n];\n";
open(my $fh, '>', $jsonFile) or die "Can't create'$jsonFile': $!";
print $fh $json;
close($fh);
if(!$gzipDiasable) {
    my $cmd = "gzip -f $jsonFile";
    system($cmd);
}
print "$jsonFile [Created]\n";
if($autoUpload2LittleFS eq 'YES' && $uploadfs) {
    my $original_dir = getcwd();
    chdir("..") or die "Can't change dir: $!";
    print "Now in dir: ", getcwd(), "\n";
    my $cmd = "pio run --target uploadfs";
    print "$cmd\n";
    system($cmd);
    chdir($original_dir) or die "Can't return to original dir: $!";
    print "Back in dir: ", getcwd(), "\n";
}
sub goProcess() {
    my $srcPath = shift;
    opendir(my $dh, $srcPath) or die "Can't open '$srcPath': $!";
    while (my $entry = readdir($dh)) {
        next if $entry eq '.' or $entry eq '..';
        my $path = "$srcPath/$entry";
        my $path__ = $path;
        $path__ =~ s/\Q$srcPath__\E//g;
        my @stat = stat($path);
        my $mtime = $stat[9];
        my $fsize = $stat[7];
        if (-d $path) {
            print "$path__... ";
            print "[Last modified: ", scalar localtime($mtime), "]\n";
            unless (-d $dataPath__ . $path__) {
                mkdir($dataPath__ . $path__) or die "Can't create folder '$dataPath__$path__': $!";
                print "$dataPath__$path__\t[Created]\n";
            } else {
                print "$dataPath__$path__\t[Already exists]\n";
            }
            my $jsonLine = "{\"path\":\"$path__\",\"mtime\":\"$mtime\"}";
            push @$json__, $jsonLine;
            goProcess($path);
        } elsif (-f $path) {
            if($path =~ /\.css$/ && $fsize > $gzipMinSize) {
                print "$path__... ";
                print "[Last modified: ", scalar localtime($mtime), "]\n";
                if($path =~ /\.min\.css$/ && $dontMinifyCSSmin eq 'YES') {
                    copy($path, $dataPath__ . $path__) or die "Can't copy file '$path' -> '$dataPath__ . $path__': $!";
                    print "Copied without minification...\n";
                } else {
                    open(INFILE, '<' . $path) or die "Can't open file '$path': $!\n";
                    open(OUTFILE, '>' . $dataPath__ . $path__) or die $dataPath__ . $path__ . " not openned!\n";
                    CSS::Minifier::minify(input => *INFILE, outfile => *OUTFILE);
                    close(INFILE);
                    close(OUTFILE);
                }
                unless($gzipDiasable) {
                    my $cmd = "gzip -f $dataPath__$path__";
                    system($cmd);
                }
                my $jsonLine = "{\"path\":\"$path__.gz\",\"mtime\":\"$mtime\"}";
                push @$json__, $jsonLine;
                print "$path__\t[DONE]\n";
            } elsif ((
                    $path =~ /\.ttf$/ || 
                    $path =~ /\.eot$/ || 
                    $path =~ /\.svg$/
                )  && $fsize > $gzipMinSize
            ) {
                print "$path__... ";
                print "[Last modified: ", scalar localtime($mtime), "]\n";
                copy($path, $dataPath__ . $path__) or die "Can't copy file '$path' -> '$dataPath__ . $path__': $!";
                unless($gzipDiasable) {
                    my $cmd = "gzip -f $dataPath__$path__";
                    system($cmd);
                }
                my $jsonLine = "{\"path\":\"$path__.gz\",\"mtime\":\"$mtime\"}";
                push @$json__, $jsonLine;
                print "$path__\t[DONE]\n";
            } elsif ($path =~ /\.js$/ && $fsize > $gzipMinSize) {
                print "$path__... ";
                print "[Last modified: ", scalar localtime($mtime), "]\n";
                if($path =~ /\.min\.js$/) {
                    copy($path, $dataPath__ . $path__) or die "Can't copy file '$path' -> '$dataPath__ . $path__': $!";
                    print "Copied without minification...\n";
                } else {
                    open(INFILE, '<' . $path) or die "Can't open file '$path': $!\n";
                    open(OUTFILE, '>' . $dataPath__ . $path__) or die "Can't open file '$dataPath__$path__': $!\n";
                    JavaScript::Minifier::minify(input => *INFILE, outfile => *OUTFILE);
                    close(INFILE);
                    close(OUTFILE);
                }
                unless($gzipDiasable) {
                    my $cmd = "gzip -f $dataPath__$path__";
                    system($cmd);
                }
                my $jsonLine = "{\"path\":\"$path__.gz\",\"mtime\":\"$mtime\"}";
                push @$json__, $jsonLine;
                print "$path__\t[DONE]\n";
            } elsif ($path =~ /\.html$/ && $fsize > $gzipMinSize) {
                print "$path__... ";
                print "[Last modified: ", scalar localtime($mtime), "]\n";
                copy($path, $dataPath__ . $path__) or die "Can't copy '$path' -> '$dataPath__ . $path__': $!";
                if(
                    $path__ eq '/index.html' || $path__ eq '/littlefs.html' || 
                    $path__ eq '/logs.html' || $path__ eq '/microsd.html' || 
                    $path__ eq '/telegram.html' || $path__ eq '/options.html'
                ) {
                    open(INF, '<' . $templatesParh__ . '/menu.html') or die "Can't open file '" 
                        .$templatesParh__ . '/menu.html' . "': $!(templates)\n";
                    my $menu = '';
                    while(my $line = <INF>) { $menu .= $line; }
                    close(INF);
                    open(INF, '<' . $dataPath__ . $path__) or die "Can't open file '$dataPath__$path__': $!(templates)\n";
                    my $content = '';
                    while(my $line = <INF>) { $content .= $line; }
                    close(INF);
                    $content =~ s/-=MENU=-/$menu/;
                    open(OUTF, '>' . $dataPath__ . $path__) or die "Can't open file '$dataPath__$path__': $!(templates)\n";
                    print OUTF $content;
                    close(OUTF);
                }
                my $jsonLine = "{\"path\":\"$path__.gz\",\"mtime\":\"$mtime\"}";
                push @$json__, $jsonLine;
                unless($gzipDiasable) {
                    my $cmd = "gzip -f $dataPath__$path__";
                    system($cmd);
                }
                print "$path__\t[DONE]\n";
            } else {
                print "$path__... ";
                print "[Last modified: ", scalar localtime($mtime), "]\n";
                copy($path, $dataPath__ . $path__) or die "Can't copy '$path' -> '$dataPath__ . $path__': $!";
                my $jsonLine = "{\"path\":\"$path__\",\"mtime\":\"$mtime\"}";
                push @$json__, $jsonLine;
                print "$path__\t[DONE]\n";
            }

        }
    }
    closedir($dh);
}