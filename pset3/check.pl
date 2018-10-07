#! /usr/bin/perl -w

# check.pl
#    This program runs the tests in io61 and stdio versions.
#    It compares their outputs and measures time and memory usage.
#    It tries to prevent disaster: if your code looks like it's
#    generating an infinite-length file, or using too much memory,
#    check.pl will kill it.
#
#    To add tests of your own, scroll down to the bottom. It should
#    be relatively clear what to do.

use Time::HiRes qw(gettimeofday);
use Fcntl qw(F_GETFL F_SETFL O_NONBLOCK);
use POSIX;
use Scalar::Util qw(looks_like_number);
use List::Util qw(shuffle);
use Config;
my($nkilled) = 0;
my($nerror) = 0;
my(@ratios, @runtimes, @basetimes, @alltests);
my(%fileinfo);
my($NOSTDIO) = exists($ENV{"NOSTDIO"});
my($NOYOURCODE) = exists($ENV{"NOYOURCODE"});
my($TRIALTIME) = exists($ENV{"TRIALTIME"}) ? $ENV{"TRIALTIME"} + 0 : 3;
my($TRIALS) = exists($ENV{"TRIALS"}) ? int($ENV{"TRIALS"}) : 5;
$TRIALS = 5 if $TRIALS <= 0;
my($STDIOTRIALS) = exists($ENV{"STDIOTRIALS"}) ? int($ENV{"STDIOTRIALS"}) : $TRIALS;
$STDIOTRIALS = $TRIALS if $STDIOTRIALS <= 0;
my($MAXTIME) = exists($ENV{"MAXTIME"}) ? $ENV{"MAXTIME"} + 0 : 20;
$MAXTIME = 20 if $MAXTIME <= 0;
my($MAKETRIALLOG) = exists($ENV{"MAKETRIALLOG"}) && $ENV{"MAKETRIALLOG"} ne "" && $ENV{"MAKETRIALLOG"} ne "0";
sub first (@) { return $_[0]; }
my($CHECKSUM) = first(grep {-x $_} ("/usr/bin/md5sum", "/sbin/md5",
                                    "/bin/false"));
my($VERBOSE) = exists($ENV{"VERBOSE"});
my($NOMAKE) = exists($ENV{"NOMAKE"}) && int($ENV{"NOMAKE"});
eval { require "syscall.ph" };

my($Red, $Redctx, $Green, $Cyan, $Off) = ("\x1b[01;31m", "\x1b[0;31m", "\x1b[01;32m", "\x1b[01;36m", "\x1b[0m");
$Red = $Redctx = $Green = $Cyan = $Off = "" if !-t STDERR || !-t STDOUT;


$SIG{"CHLD"} = sub {};
my($run61_pid);

sub decache ($) {
    my($fn) = @_;
    if (defined(&{"SYS_fadvise64"}) && open(DECACHE, "<", $fn)) {
        syscall &SYS_fadvise64, fileno(DECACHE), 0, -s DECACHE, 4;
        close(DECACHE);
    }
}

sub makefile ($) {
    my($filename) = @_;
    my($size) = $fileinfo{$filename}->[2];
    my($cmd) = $filename =~ /-rev/ ? "rev" : "cat";
    my($src) = $filename =~ /\.bin$/ ? "/bin/sh" : "/usr/share/dict/words";
    my($first_offset) = "";
    if (defined($fileinfo{$filename}->[3]) && $fileinfo{$filename}->[3]) {
        $first_offset = " | tail -c +" . $fileinfo{$filename}->[3];
    }
    if (!-r $filename || !defined(-s $filename) || -s $filename != $size) {
        while (!defined(-s $filename) || -s $filename < $size) {
            system("$cmd $src$first_offset >> $filename");
            $first_offset = "";
        }
        truncate($filename, $size);
    }
    $fileinfo{$filename} = [-M $filename, -C $filename, $size];
}

sub verify_file ($) {
    my($filename) = @_;
    if (exists($fileinfo{$filename})
        && (!-r $filename
            || $fileinfo{$filename}->[0] != -M $filename
            || $fileinfo{$filename}->[1] != -C $filename)) {
        truncate($filename, 0);
        makefile($filename);
    }
    return -s $filename;
}

sub file_md5sum ($) {
    my($x) = `$CHECKSUM $_[0]`;
    $x =~ s{\A(\S+).*\z}{$1}s;
    return $x;
}

sub run_sh61_pipe ($$;$) {
    my($text, $fd, $size) = @_;
    my($n, $buf) = (0, "");
    return $text if !defined($fd);
    while ((!defined($size) || length($text) <= $size)
           && defined(($n = POSIX::read($fd, $buf, 8192)))
           && $n > 0) {
        $text .= substr($buf, 0, $n);
    }
    return $text;
}

sub run_sh61 ($;%) {
    my($command, %opt) = @_;
    my($outfile) = exists($opt{"stdout"}) ? $opt{"stdout"} : undef;
    my($size_limit_file) = exists($opt{"size_limit_file"}) ? $opt{"size_limit_file"} : $outfile;
    $size_limit_file = [$size_limit_file] if $size_limit_file && !ref($size_limit_file);
    my($size_limit) = exists($opt{"size_limit"}) ? $opt{"size_limit"} : undef;
    my($dir) = exists($opt{"dir"}) ? $opt{"dir"} : undef;
    if (defined($dir) && $size_limit_file) {
        $dir =~ s{/+$}{};
        $size_limit_file = [map { m{^/} ? $_ : "$dir/$_" } @$size_limit_file];
    }
    pipe(PR, PW);
    pipe(OR, OW) or die "pipe";
    fcntl(OR, F_SETFL, fcntl(OR, F_GETFL, 0) | O_NONBLOCK);
    1 while waitpid(-1, WNOHANG) > 0;

    $run61_pid = fork();
    if ($run61_pid == 0) {
        POSIX::setpgid(0, 0) or die("child setpgid: $!\n");
        defined($dir) && chdir($dir);

        close(PR);
        POSIX::dup2(fileno(PW), 100);
        close(PW);

        close(OR);
        POSIX::dup2(fileno(OW), 1);
        POSIX::dup2(fileno(OW), 2);
        close(OW) if fileno(OW) != 1 && fileno(OW) != 2;

        fcntl(STDIN, F_SETFD, fcntl(STDIN, F_GETFD, 0) & ~FD_CLOEXEC);
        fcntl(STDOUT, F_SETFD, fcntl(STDOUT, F_GETFD, 0) & ~FD_CLOEXEC);
        fcntl(STDERR, F_SETFD, fcntl(STDERR, F_GETFD, 0) & ~FD_CLOEXEC);

        { exec($command) };
        print STDERR "error trying to run $command: $!\n";
        exit(1);
    }

    POSIX::setpgid($run61_pid, $run61_pid) or die("setpgid: $!\n");

    my($before) = Time::HiRes::time();
    my($died) = 0;
    my($max_time) = exists($opt{"time_limit"}) ? $opt{"time_limit"} : 0;
    my($out, $buf, $nb) = ("", "");
    my($answer) = exists($opt{"answer"}) ? $opt{"answer"} : {};
    $answer->{"command"} = $command;

    close(PW);
    close(OW);

    eval {
        do {
            Time::HiRes::usleep(300000);
            if (waitpid($run61_pid, WNOHANG) > 0) {
                $answer->{"status"} = $?;
                die "!";
            }
            if (defined($size_limit) && $size_limit_file && @$size_limit_file) {
                my($len) = 0;
                $out = run_sh61_pipe($out, fileno(OR), $size_limit);
                foreach my $fname (@$size_limit_file) {
                    $len += ($fname eq "pipe" ? length($out) : -s $fname);
                }
                if ($len > $size_limit) {
                    $died = "output file size $len, expected <= $size_limit";
                    die "!";
                }
            }
        } while (Time::HiRes::time() < $before + $max_time);
        $died = sprintf("timeout after %.2fs", $max_time)
            if waitpid($run61_pid, WNOHANG) <= 0;
    };

    my($delta) = Time::HiRes::time() - $before;
    $answer->{"time"} = $delta;

    if (exists($answer->{"status"}) && exists($opt{"delay"}) && $opt{"delay"} > 0) {
        Time::HiRes::usleep($opt{"delay"} * 1e6);
    }
    if (exists($opt{"nokill"})) {
        $answer->{"pgrp"} = $run61_pid;
    } else {
        kill 9, -$run61_pid;
    }
    $run61_pid = 0;

    if ($died) {
        $answer->{"killed"} = $died;
        close(OR);
        close(PR);
        return $answer;
    }

    $nb = POSIX::read(fileno(PR), $buf, 2000);
    close(PR);
    $buf = $nb > 0 ? substr($buf, 0, $nb) : "";

    while ($buf =~ m,\"(.*?)\"\s*:\s*([\d.]+),g) {
        $answer->{$1} = $2;
    }
    $answer->{"time"} = $delta if !defined($answer->{"time"});
    $answer->{"time"} = $delta if $answer->{"time"} <= 0.95 * $delta;
    $answer->{"utime"} = $delta if !defined($answer->{"utime"});
    $answer->{"stime"} = $delta if !defined($answer->{"stime"});
    $answer->{"maxrss"} = -1 if !defined($answer->{"maxrss"});

    $out = run_sh61_pipe($out, fileno(OR), $size_limit);
    close(OR);

    if ($size_limit_file && @$size_limit_file) {
        my($len, @sums) = 0;
        foreach my $fname (@$size_limit_file) {
            my($sz) = $fname eq "pipe" ? length($out) : -s $fname;
            $len += $sz if defined($sz);
            if ($VERBOSE && $fname eq "pipe") {
                # XXX
            } elsif (($VERBOSE || $MAKETRIALLOG || exists($ENV{"TRIALLOG"}))
                     && -f $fname
                     && (!exists($opt{"no_content_check"}) || !$opt{"no_content_check"})) {
                push @sums, file_md5sum($fname);
            }
        }
        $answer->{"outputsize"} = $len;
        $answer->{"md5sum"} = join(" ", @sums) if @sums;
    }

    my(@stderr);
    if ($out) {
        my($tx) = "";
        foreach my $l (split(/\n/, $out)) {
            $tx .= ($tx eq "" ? "" : "        : ") . $l . "\n" if $l ne "";
        }
        if ($tx ne "" && exists($answer->{"trial"})) {
            push @stderr, "    ${Redctx}STDERR (trial " . $answer->{"trial"} . "): $tx${Off}";
        } elsif ($tx ne "") {
            push @stderr, "    ${Redctx}STDERR: $tx${Off}";
        }
    }
    if (exists($answer->{"status"}) && ($answer->{"status"} & 255)) {
        my(@signames) = split(' ', $Config{"sig_name"});
        my($signame) = $signames[$answer->{"status"} & 255];
        $signame = defined($signame) ? "$signame signal" : "signal " . ($answer->{"status"} & 255);
        if (exists($answer->{"trial"})) {
            push @stderr, "    ${Redctx}KILLED by $signame (trial " . $answer->{"trial"} . ")\n${Off}";
        } else {
            push @stderr, "    ${Redctx}KILLED by $signame\n${Off}";
        }
    }
    $answer->{"stderr"} = join("\n", @stderr) if @stderr;

    return $answer;
}

sub read_triallog ($) {
    my($buf);
    open(TRIALLOG, "<", $_[0]) or die "$_[0]: $!\n";
    while (defined($buf = <TRIALLOG>)) {
        my($t) = {};
        while ($buf =~ m,"([^"]*)"\s*:\s*([\d.]+),g) {
            $t->{$1} = $2 + 0;
        }
        while ($buf =~ m,"([^"]*)"\s*:\s*"([^"]*)",g) {
            $t->{$1} = $2;
        }
        push @alltests, $t if keys(%$t);
    }
    close(TRIALLOG);
}

sub maybe_make ($) {
    my($command) = @_;
    if (!$NOMAKE && $command =~ m<(?:^|[|&;]\s*)./(\S+)>) {
        $verbose = defined($ENV{"V"}) && $ENV{"V"} && $ENV{"V"} ne "0";
        if (system($verbose ? "make $1" : "make -s $1") != 0) {
            print STDERR "${Red}ERROR: Cannot make $1${Off}\n";
            exit 1;
        }
    }
}

my(@workq, %command_max_size, %command_trials);

sub find_tests ($$$) {
    my($number, $type, $command) = @_;
    grep {
        $_->{"number"} == $number && $_->{"type"} eq $type
            && $_->{"command"} eq $command
    } @alltests;
}

sub enqueue ($$$%) {
    my($number, $command, $desc, %opt) = @_;
    return if (@ARGV && !grep {
        ($_ =~ m{^\s*(?:0b[01]+|0[0-7]*|0x[0-9a-fA-F]+|[0-9]*)\s*$}
         && $_ == $number)
            || ($_ =~ m{^(\d+)-(\d+)$} && $number >= $1 && $number <= $2)
            || ($_ =~ m{(?:^|,)$number(,|$)})
               } @ARGV);
    my($expansion) = $opt{"expansion"};
    $expansion = 1 if !$expansion;

    # verify input files
    my(@infiles);
    my($insize) = 0;
    while ($command =~ m{\b([-a-z0-9/]+\.(?:txt|bin))\b}g) {
        if (exists($fileinfo{$1})) {
            push @infiles, $1;
            $insize += verify_file($1);
        }
    }
    my($outsuf) = ".txt";
    $outsuf = ".bin" if $command =~ m<out\.bin>;
    my($no_content_check) = exists($opt{"no_content_check"});

    # prepare stdio command
    my($stdiocmd) = $command;
    $stdiocmd =~ s<(\./)([a-z]*61)><${1}stdio-$2>g;
    $stdiocmd =~ s<out(\d*)\.(txt|bin)><baseout$1\.$2>g;
    my($stdio_qitem) = {
        "test_number" => $number, "desc" => $desc, "type" => "stdio",
        "command" => $stdiocmd,
        "maincommand" => $command, "stdiocommand" => $stdiocmd,
        "count" => 0, "elapsed" => 0, "errors" => 0, "nleft" => $STDIOTRIALS,
        "infiles" => \@infiles, "outfiles" => [],
        "insize" => $insize, "check_max_size" => 0, "opt" => \%opt,
        "no_content_check" => $no_content_check
    };
    while ($stdiocmd =~ m{([^\s<>]*baseout\d*\.(?:txt|bin))}g) {
        push @{$stdio_qitem->{"outfiles"}}, $1;
    }
    for (my $i = 0; !$NOSTDIO && $i < $STDIOTRIALS; ++$i) {
        push @workq, $stdio_qitem;
    }

    # prepare maximum size, possibly using triallog
    $command_max_size{$command} = $expansion * $insize;
    foreach my $t (find_tests($number, "stdio", $stdiocmd)) {
        if (exists($t->{"outputsize"})
            && $t->{"outputsize"} > $command_max_size{$command}) {
            $command_max_size{$command} = $t->{"outputsize"};
        }
    }

    # prepare normal command
    my($your_qitem) = {
        "test_number" => $number, "desc" => $desc, "type" => "yourcode",
        "command" => $command,
        "maincommand" => $command, "stdiocommand" => $stdiocmd,
        "count" => 0, "elapsed" => 0, "errors" => 0, "nleft" => $TRIALS,
        "infiles" => \@infiles, "outfiles" => [],
        "insize" => $insize, "check_max_size" => 1, "opt" => \%opt,
        "no_content_check" => $no_content_check
    };
    while ($command =~ m{([^\s<>]*out\d*\.(?:txt|bin))}g) {
        push @{$your_qitem->{"outfiles"}}, $1;
    }
    for (my $i = 0; !$NOYOURCODE && $i < $TRIALS; ++$i) {
        push @workq, $your_qitem;
    }
    $command_trials{$command} = ($NOSTDIO ? 0 : $STDIOTRIALS)
        + ($NOYOURCODE ? 0 : $TRIALS);
}

sub run_qitem ($) {
    my($qitem) = @_;
    $qitem->{"nleft"} -= 1;
    $command_trials{$qitem->{"maincommand"}} -= 1;
    return 0 if $TRIALTIME > 0 && $qitem->{"elapsed"} >= $TRIALTIME;
    return 0 if $qitem->{"errors"} > 1;

    foreach my $f (@{$qitem->{"infiles"}}) {
        decache($f);
    }
    Time::HiRes::usleep(100000);

    my($time_limit) = $qitem->{"type"} eq "stdio" ? 60 : $MAXTIME;
    my($max_size) = undef;
    if ($qitem->{"check_max_size"}) {
        $max_size = $command_max_size{$qitem->{"maincommand"}} * 2;
    }
    my($t) = run_sh61($qitem->{"command"},
                      "size_limit_file" => $qitem->{"outfiles"},
                      "time_limit" => $time_limit,
                      "size_limit" => $max_size,
                      "answer" => {"number" => $qitem->{"test_number"},
                                   "type" => $qitem->{"type"},
                                   "trial" => $qitem->{"count"} + 1},
                      "no_content_check" => $qitem->{"no_content_check"});
    push @alltests, $t;

    $qitem->{"count"} += 1;
    $qitem->{"errors"} += 1 if exists($t->{"killed"});
    $qitem->{"elapsed"} += $t->{"time"};

    if (!$max_size && exists($t->{"outputsize"})
        && $t->{"outputsize"} > $command_max_size{$qitem->{"maincommand"}}) {
        $command_max_size{$qitem->{"maincommand"}} = $t->{"outputsize"};
    }

    $t;
}

sub median_trial ($$$;$) {
    my($number, $type, $qitem, $tcompar) = @_;
    my $command = $qitem->{$type eq "stdio" ? "stdiocommand" : "maincommand"};
    my(@tests) = find_tests($number, $type, $command);
    return undef if !@tests;

    # return error test if more than one error observed
    my(@errortests) = grep { exists($_->{"killed"}) } @tests;
    if (@errortests > 1) {
        $errortests[0]->{"medianof"} = scalar(@tests);
        $errortests[0]->{"stderr"} = "" if !exists($errortests[0]->{"stderr"});
        return $errortests[0];
    }

    # collect stderr and md5sum from all tests
    my($stderr) = join("", map {
                           exists($_->{"stderr"}) ? $_->{"stderr"} : ""
                       } @tests);
    my(%md5sums) = map {
        exists($_->{"md5sum"}) ? ($_->{"md5sum"} => 1) : ()
    } @tests;
    my(%outputsizes) = map {
        exists($_->{"outputsize"}) ? ($_->{"outputsize"} => 1) : ()
    } @tests;

    # pick median test (or another test that's not erroneous)
    @tests = sort { $a->{"time"} <=> $b->{"time"} } @tests;
    my $tt = {};
    my $tidx = int(@tests / 2);
    for (my $j = 0; $j < @tests; ++$j) {
        %$tt = %{$tests[$tidx]};

        if ($tcompar) {
            if (exists($tcompar->{"outputsize"})
                && exists($tt->{"outputsize"})
                && $tt->{"outputsize"} != $tcompar->{"outputsize"}) {
                $tt->{"different_size"} = 1;
            }
            if (exists($tcompar->{"content_check"})) {
                foreach my $fname (@{$tcompar->{"content_check"}}) {
                    my($basefname) = $fname;
                    $basefname =~ s{files/}{files/base};
                    my($r) = scalar(`cmp $basefname $fname 2>&1`);
                    chomp $r if $r;
                    $r =~ s/^cmp: // if $r;
                    $tt->{"different_content"} = " ($r)" if $?;
                }
            }
            if (exists($tcompar->{"md5sum_check"}) && exists($tt->{"md5sum"})) {
                $tt->{"different_content"} = " (got md5sum " . $tt->{"md5sum"}
                    . ", expected " . $tcompar->{"md5sum_check"} . ")"
                    if $tcompar->{"md5sum_check"} ne $tt->{"md5sum"};
            }
        }

        last if !exists($tt->{"killed"}) && !exists($tt->{"different_size"})
            && !exists($tt->{"different_content"});

        $tidx = ($tidx + 1) % @tests;
    }

    # decorate it
    $tt->{"medianof"} = scalar(@tests);
    $tt->{"stderr"} = $stderr;
    if (keys(%md5sums) == 1) {
        $tt->{"md5sum"} = (keys(%md5sums))[0];
    }
    if (keys(%outputsizes) > 1 || keys(%md5sums) > 1) {
        $tt->{"stderr"} .= "    ${Red}ERROR: trial runs generated different output${Off}\n";
    }
    return $tt;
}

sub print_stdio ($) {
    my($t) = @_;
    if (exists($t->{"utime"})) {
        printf("%.5fs (%.5fs user, %.5fs system, %dKiB memory, %d trial%s)\n",
               $t->{"time"}, $t->{"utime"}, $t->{"stime"}, $t->{"maxrss"},
               $t->{"medianof"}, $t->{"medianof"} == 1 ? "" : "s");
    } else {
        printf("${Red}KILLED${Redctx} after %.5fs (%d trial%s)${Off}\n",
               $t->{"time"},
               $t->{"medianof"}, $t->{"medianof"} == 1 ? "" : "s");
    }
}

sub run ($) {
    my($sequentially) = @_;
    my($number, $type) = (0, undef);
    @workq = shuffle(@workq) if !$sequentially;
    my($stdiot);

    for (my $qpos = 0; $qpos < @workq; $qpos += 1) {
        my($qitem) = $workq[$qpos];

        # print header
        if ($number != $qitem->{"test_number"} || !$sequentially) {
            $number = $qitem->{"test_number"};
            print "TEST:      $number. ", $qitem->{"desc"}, "\n";
            print "COMMAND:   ", $qitem->{"maincommand"}, "\n"
                if !exists($ENV{"NOCOMMAND"});
            $type = "";
            $stdiot = undef;
        }
        if (($type ne $qitem->{"type"}) && $sequentially
            && $qitem->{"type"} eq "yourcode"
            && ($stdiot = median_trial($number, "stdio", $qitem))) {
            print "STDIO:     " if $NOSTDIO;
            print_stdio($stdiot);
        }
        maybe_make($qitem->{"command"});
        if ($type ne $qitem->{"type"}) {
            $type = $qitem->{"type"};
            print ($type eq "stdio" ? "STDIO:     " : "YOUR CODE: ");
        }

        # run it
        my($t) = run_qitem($qitem);
        if (!$sequentially) {
            if (!$t) {
                printf("${Redctx}skipped${Off}\n");
            } elsif (exists($t->{"utime"})) {
                printf("%.5fs (%.5fs user, %.5fs system, %dKiB memory)\n",
                   $t->{"time"}, $t->{"utime"}, $t->{"stime"}, $t->{"maxrss"});
            } else {
                printf("${Red}KILLED${Redctx} after %.5fs${Off}\n",
                       $t->{"time"});
            }
        }
        next if $command_trials{$qitem->{"maincommand"}};

        # print results
        if (!$sequentially) {
            print "\nTEST SUMMARY: ", $number, ". ", $qitem->{"desc"}, "\n";
            if (($stdiot = median_trial($number, "stdio", $qitem))) {
                print "STDIO:     ";
                print_stdio($stdiot);
            }
            print "YOUR CODE: ";
        }
        my($tcompar) = {};
        $tcompar->{"outputsize"} = $stdiot->{"outputsize"}
            if $stdiot && exists($stdiot->{"outputsize"});
        if (!$NOYOURCODE && !$qitem->{"no_content_check"}) {
            $tcompar->{"content_check"} = $qitem->{"outfiles"}
                if !$NOSTDIO && $sequentially;
            $tcompar->{"md5sum_check"} = $stdiot->{"md5sum"}
                if $NOSTDIO && $stdiot && exists($stdiot->{"md5sum"});
        }

        my($tt) = median_trial($number, "yourcode", $qitem, $tcompar);
        print "YOUR CODE: " if $tt && $NOYOURCODE;
        if ($tt && exists($tt->{"killed"})) {
            printf "${Red}KILLED${Redctx} (%s)${Off}\n", $tt->{"killed"};
            ++$nkilled;
        } elsif ($tt) {
            printf("%.5fs (%.5fs user, %.5fs system, %dKiB memory, %d trial%s)\n",
               $tt->{"time"}, $tt->{"utime"}, $tt->{"stime"}, $tt->{"maxrss"},
               $tt->{"medianof"}, $tt->{"medianof"} == 1 ? "" : "s");
            push @runtimes, $tt->{"time"};
        }

        # print stdio vs. yourcode comparison
        if ($stdiot && $tt && $tt->{"time"} && !exists($tt->{"killed"})
            && !exists($tt->{"different_size"})
            && !exists($tt->{"different_content"})) {
            my($ratio) = $stdiot->{"time"} / $tt->{"time"};
            my($color) = ($ratio < 0.5 ? $Redctx : ($ratio > 1.9 ? $Green : $Cyan));
            printf("RATIO:     ${color}%.2fx stdio${Off}\n", $ratio);
            push @ratios, $ratio;
            push @basetimes, $stdiot->{"time"};
        }
        if (exists($tt->{"different_size"})) {
            print "    ${Red}ERROR: ", join("+", @{$qitem->{"outfiles"}}), " has size ", $tt->{"outputsize"}, ", expected ", $stdiot->{"outputsize"}, "${Off}\n";
        }
        if (exists($tt->{"different_content"})) {
            my(@xoutfiles) = map {s{^files/}{}; $_} @{$qitem->{"outfiles"}};
            print "    ${Red}ERROR: ", join("+", @xoutfiles),
                " differs from stdio's ", join("+", map {"base$_"} @xoutfiles),
                "${Redctx}", $tt->{"different_content"}, "$Off\n";
        }
        ++$nerror if exists($tt->{"different_content"}) || exists($tt->{"different_size"});

        # print yourcode stderr and a blank-line separator
        print $tt->{"stderr"} if exists($tt->{"stderr"}) && $tt->{"stderr"} ne "";
        print "\n";
    }
}

sub pl ($$) {
    my($n, $x) = @_;
    return $n . " " . ($n == 1 ? $x : $x . "s");
}

sub summary () {
    my($ntests) = @runtimes + $nkilled;
    print "SUMMARY:   ", pl($ntests, "test"), ", ";
    if ($nkilled) {
        print "${Red}$nkilled killed,${Off} ";
    } else {
        print "0 killed, ";
    }
    if ($nerror) {
        print "${Red}", pl($nerror, "error"), "${Off}\n";
    } else {
        print "0 errors\n";
    }
    my($better) = scalar(grep { $_ > 1 } @ratios);
    my($worse) = scalar(grep { $_ < 1 } @ratios);
    if ($better || $worse) {
        print "           better than stdio ", pl($better, "time"),
        ", worse ", pl($worse, "time"), "\n";
    }
    my($mean, $basetime, $runtime) = (0, 0, 0);
    for (my $i = 0; $i < @ratios; ++$i) {
        $mean += $ratios[$i];
        $basetime += $basetimes[$i];
    }
    for (my $i = 0; $i < @runtimes; ++$i) {
        $runtime += $runtimes[$i];
    }
    if (@ratios) {
        printf "           average %.2fx stdio\n", $mean / @ratios;
        printf "           total time %.3fs stdio, %.3fs your code (%.2fx stdio)\n",
        $basetime, $runtime, $basetime / $runtime;
    } elsif (@runtimes) {
        printf "           total time %.3f your code\n", $runtime;
    }

    if ($VERBOSE || $MAKETRIALLOG) {
        my(@testjsons);
        foreach my $t (@alltests) {
            my(@tout, $k, $v) = ();
            while (($k, $v) = each %$t) {
                push @tout, "\"$k\":" . (looks_like_number($v) ? $v : "\"$v\"");
            }
            push @testjsons, "{" . join(",", @tout) . "}\n";
        }
        print "\n", @testjsons if $VERBOSE;
        if ($MAKETRIALLOG) {
            open(OTRIALLOG, ">", $ENV{"MAKETRIALLOG"} eq "1" ? "triallog.txt" : $ENV{"MAKETRIALLOG"}) or die;
            print OTRIALLOG @testjsons;
            close(OTRIALLOG);
        }
    }
}

# maybe read a trial log
if (exists($ENV{"TRIALLOG"})) {
    read_triallog($ENV{"TRIALLOG"});
}

# create some files
if (!-d "files" && (-e "files" || !mkdir("files"))) {
    print STDERR "*** Cannot run tests because 'files' cannot be created.\n";
    print STDERR "*** Remove 'files' and try again.\n";
    exit(1);
}
$fileinfo{"files/text1meg.txt"} = [0, 0, 1 << 20, 0];
$fileinfo{"files/text90k-rev.txt"} = [0, 0, 90 << 10, 2 << 10];
$fileinfo{"files/binary1meg.bin"} = [0, 0, 1 << 20, 3 << 10];
$fileinfo{"files/text5meg.txt"} = [0, 0, 5 << 20, 4 << 10];
$fileinfo{"files/text20meg.txt"} = [0, 0, 20 << 20, 5 << 10];

$SIG{"INT"} = sub {
    kill 9, -$run61_pid if $run61_pid;
    summary();
    exit(1);
};

my($sequentially) = 1;
while (@ARGV) {
    if ($ARGV[0] eq "-r") {
        $sequentially = 0;
    } elsif ($ARGV[0] eq "-V") {
        $VERBOSE = 1;
    } else {
        last;
    }
    shift @ARGV;
}


# REGULAR FILES, SEQUENTIAL I/O
enqueue(1,
    "./cat61 -o files/out.txt files/text1meg.txt",
    "regular small file, character I/O, sequential");

enqueue(2,
    "./cat61 -o files/out.bin files/binary1meg.bin",
    "regular small binary file, character I/O, sequential");

enqueue(3,
    "./cat61 -o files/out.txt files/text20meg.txt",
    "regular large file, character I/O, sequential");

enqueue(4,
    "./blockcat61 -b 1024 -o files/out.txt files/text5meg.txt",
    "regular medium file, 1KB block I/O, sequential");

enqueue(5,
    "./blockcat61 -o files/out.txt files/text20meg.txt",
    "regular large file, 4KB block I/O, sequential");

enqueue(6,
    "./blockcat61 -o files/out.bin files/binary1meg.bin",
    "regular small binary file, 4KB block I/O, sequential");

enqueue(7,
    "./randblockcat61 -o files/out.txt files/text20meg.txt",
    "regular large file, 1B-4KB block I/O, sequential");

enqueue(8,
    "./randblockcat61 -r 6582 -o files/out.txt files/text20meg.txt",
    "regular large file, 1B-4KB block I/O, sequential");

enqueue(9,
    "./blockcat61 -b 509 -o files/out.txt files/text5meg.txt",
    "regular medium file, 509B block I/O, sequential");


# MULTIPLE REGULAR FILES (mostly correctness tests)

enqueue(10,
    "./scattergather61 -b 512 -o files/out.bin files/binary1meg.bin files/text1meg.txt",
    "gathered small files, 512B block I/O, sequential");

enqueue(11,
    "./scattergather61 -b 512 -o files/out1.txt -o files/out2.txt -o files/out3.txt < files/text1meg.txt",
    "scattered small file, 512B block I/O, sequential");

enqueue(12,
    "./scattergather61 -b 509 -o files/out1.txt -o files/out2.txt -o files/out3.txt -o files/out4.txt -i files/text1meg.txt -i files/text90k-rev.txt -i files/text1meg.txt",
    "scatter/gather 4/3 files, 509B block I/O, sequential");

enqueue(13,
    "./scattergather61 -b 128 -l -o files/out1.txt -o files/out2.txt -o files/out3.txt -o files/out4.txt -i files/text1meg.txt -i files/text90k-rev.txt -i files/text1meg.txt",
    "scatter/gather 4/3 files by lines, character I/O, sequential");


# REGULAR FILES, REVERSE I/O

enqueue(14,
    "./reverse61 -o files/out.txt files/text5meg.txt",
    "regular medium file, character I/O, reverse order");

enqueue(15,
    "./reverse61 -o files/out.txt files/text20meg.txt",
    "regular large file, character I/O, reverse order");


# FUNNY FILES

enqueue(16,
    "./cat61 -s 4096 -o files/out.txt /dev/urandom",
    "seekable unmappable file, character I/O, sequential",
    "no_content_check" => 1);

enqueue(17,
    "./reverse61 -s 4096 -o files/out.txt /dev/urandom",
    "seekable unmappable file, character I/O, reverse order",
    "no_content_check" => 1);

enqueue(18,
    "./cat61 -s 5242880 -o files/out.txt /dev/zero",
    "magic zero file, character I/O, sequential");

enqueue(19,
    "./reverse61 -s 5242880 -o files/out.txt /dev/zero",
    "magic zero file, character I/O, reverse order");


# STRIDE AND REORDER I/O PATTERNS

enqueue(20,
    "./reordercat61 -o files/out.txt files/text20meg.txt",
    "regular large file, 4KB block I/O, random seek order");

enqueue(21,
    "./reordercat61 -r 6582 -o files/out.txt files/text20meg.txt",
    "regular large file, 4KB block I/O, random seek order");

enqueue(22,
    "./stridecat61 -t 1048576 -o files/out.txt files/text5meg.txt",
    "regular medium file, character I/O, 1MB stride order");

enqueue(23,
    "./stridecat61 -t 2 -o files/out.txt files/text5meg.txt",
    "regular medium file, character I/O, 2B stride order");


# PIPE FILES, SEQUENTIAL I/O

enqueue(24,
    "cat files/text1meg.txt | ./cat61 | cat > files/out.txt",
    "piped small file, character I/O, sequential");

enqueue(25,
    "cat files/text20meg.txt | ./cat61 | cat > files/out.txt",
    "piped large file, character I/O, sequential");

enqueue(26,
    "cat files/text5meg.txt | ./blockcat61 -b 1024 | cat > files/out.txt",
    "piped medium file, 1KB block I/O, sequential");

enqueue(27,
    "cat files/text20meg.txt | ./blockcat61 | cat > files/out.txt",
    "piped large file, 4KB block I/O, sequential");

enqueue(28,
    "cat files/text20meg.txt | ./randblockcat61 | cat > files/out.txt",
    "piped large file, 1B-4KB block I/O, sequential");

enqueue(29,
    "./blockcat61 -b 1024 files/text5meg.txt | cat > files/out.txt",
    "mixed-piped medium file, 1KB block I/O, sequential");

enqueue(30,
    "cat files/text20meg.txt | ./blockcat61 -o files/out.txt",
    "mixed-piped large file, 4KB block I/O, sequential");

enqueue(31,
    "./randblockcat61 files/text20meg.txt > files/out.txt",
    "redirected large file, 1B-4KB block I/O, sequential");


run($sequentially);

summary();
