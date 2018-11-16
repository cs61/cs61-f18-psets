#! /usr/bin/perl -w
use Time::HiRes qw(gettimeofday);
use Fcntl qw(F_GETFL F_SETFL O_NONBLOCK);
use POSIX;
use Scalar::Util qw(looks_like_number);
use List::Util qw(shuffle min max);
use Config;
sub first (@) { return $_[0]; }
my($CHECKSUM) = first(grep {-x $_} ("/usr/bin/md5sum", "/sbin/md5",
                                    "/bin/false"));

my($Red, $Redctx, $Green, $Cyan, $Off) = ("\x1b[01;31m", "\x1b[0;31m", "\x1b[01;32m", "\x1b[01;36m", "\x1b[0m");
$Red = $Redctx = $Green = $Cyan = $Off = "" if !-t STDERR || !-t STDOUT;


$SIG{"CHLD"} = sub {};
$SIG{"TTOU"} = "IGNORE";
my($run61_pid);

open(FOO, "sh61.cc") || die "Did you delete sh61.cc?";
$lines = 0;
$lines++ while defined($_ = <FOO>);
close FOO;

my $rev = 'rev';
my $ALLOW_OPEN = 1;
my $ALLOW_SECRET = 0;
my @ALLOW_TESTS = ();

sub CMD_INIT ()           { "CMD_INIT" }
sub CMD_CLEANUP ()        { "CMD_CLEANUP" }
sub CMD_CAREFUL_CLEANUP () { "CMD_CAREFUL_CLEANUP" }
sub CMD_FILE ()           { "CMD_FILE" }
sub CMD_OUTPUT_FILTER ()  { "CMD_OUTPUT_FILTER" }
sub CMD_INT_DELAY ()      { "CMD_INT_DELAY" }
sub CMD_SECRET ()         { "CMD_SECRET" }
sub CMD_CAT ()            { "CMD_CAT" }
sub CMD_MAX_TIME ()       { "CMD_MAX_TIME" }

@tests = (
# Execute
    [ # 0. Test title
      # 1. Test command
      # 2. Expected test output (with newlines changed to spaces)
      # 3. (optional) Setup command. This sets up the test environment,
      #    usually by creating input files. It's run by the normal shell,
      #    not your shell.
      # 4. (optional) Cleanup command. This is run, by the normal shell,
      #    after your shell has finished. It usually examines output files.
      # In the commands, the special syntax '%%' is replaced with the
      # test number.
      'Test SIMPLE1',
      'echo Hooray',
      'Hooray' ],

    [ 'Test SIMPLE2',
      'echo Double Hooray',
      'Double Hooray' ],

    [ 'Test SIMPLE3',
      'cat f3.txt',
      'Triple Hooray',
      CMD_INIT => 'echo Triple Hooray > f3.txt' ],

    [ 'Test SIMPLE4',
      "echo Line 1\necho Line 2\necho Line 3",
      'Line 1 Line 2 Line 3' ],


# Background commands
    [ 'Test BG1',
      'cp f%%a.txt f%%b.txt &',
      'Copied',
      CMD_INIT => 'echo Copied > f%%a.txt; echo Original > f%%b.txt',
      CMD_CLEANUP => 'sleep 0.1 && cat f%%b.txt' ],

    [ 'Test BG2',
      'sh -c "sleep 0.2; test -r f%%b.txt && rm -f f%%a.txt" &',
      'Still here',
      CMD_INIT => 'echo Still here > f%%a.txt; echo > f%%b.txt',
      CMD_CLEANUP => 'rm f%%b.txt && sleep 0.3 && cat f%%a.txt' ],

    # Check that background commands are run in the background
    [ 'Test BG3',
      'sleep 2 &',
      '1',
      CMD_CLEANUP => 'ps T | grep sleep | grep -v grep | head -n 1 | wc -l' ],


# Command lists
    [ 'Test LIST1',
      'echo Semi ;',
      'Semi' ],

    [ 'Test LIST2',
      'echo Semi ; echo Colon',
      'Semi Colon' ],

    [ 'Test LIST3',
      'echo Semi ; echo Colon ; echo Rocks',
      'Semi Colon Rocks' ],

    [ 'Test LIST4',
      'echo Hello ;   echo There ; echo Who ; echo Are ; echo You ; echo ?',
      'Hello There Who Are You ?' ],

    [ 'Test LIST5',
      'rm -f f%%.txt ; echo Removed',
      'Removed',
      CMD_INIT => 'echo > f%%.txt'],

    [ 'Test LIST6',
      '../sh61 -q cmd%%.sh &',
      'Hello 1',
      CMD_INIT => 'echo "echo Hello; sleep 0.4" > cmd%%.sh',
      CMD_CLEANUP => 'sleep 0.2 ; ps T | grep sleep | grep -v grep | head -n 1 | wc -l'],

    [ 'Test LIST7',
      '../sh61 -q cmd%%.sh',
      'Hello Bye 1',
      CMD_INIT => 'echo "echo Hello; sleep 2& echo Bye" > cmd%%.sh',
      CMD_CLEANUP => 'ps | grep sleep | grep -v grep | head -n 1 | wc -l'],

    [ 'Test LIST8',
      'sh -c "sleep 0.2; echo Second" & sh -c "sleep 0.15; echo First" & sleep 0.3',
      'First Second' ],


# Conditionals
    [ 'Test COND1',
      'true && echo True',
      'True' ],

    [ 'Test COND2',
      'echo True || echo False',
      'True' ],

    [ 'Test COND3',
      'grep -cv NotThere ../sh61.cc && echo Wanted',
      "$lines Wanted" ],

    [ 'Test COND4',
      'grep -c NotThere ../sh61.cc && echo Unwanted',
      '0' ],

    [ 'Test COND5',
      'false || echo True',
      'True' ],

    [ 'Test COND6',
      'true && false || true && echo Good',
      'Good' ],

    [ 'Test COND7',
      'echo Start && false || false && echo Bad',
      'Start' ],

    [ 'Test COND8',
      'sleep 0.2 && echo Second & sleep 0.1 && echo First',
      'First Second',
      CMD_CLEANUP => 'sleep 0.25'],

    [ 'Test COND9',
      'echo Start && false || false && false || echo End',
      'Start End' ],

    [ 'Test COND10',
      'false && echo no && echo no && echo no && echo no || echo yes',
      'yes' ],

    [ 'Test COND11',
      'true || echo no || echo no || echo no || echo no && echo yes',
      'yes' ],

    [ 'Test COND12', # Background conditionals
      'sleep 0.2 && echo second & echo first',
      'first second',
      CMD_CLEANUP => 'sleep 0.2' ],

    [ 'Test COND13',
      'echo first && sleep 0.1 && echo third & sleep 0.05 ; echo second ; sleep 0.1 ; echo fourth',
      'first second third fourth' ],


# Pipelines
    [ 'Test PIPE1',
      'echo Pipe | wc -c',
      '5' ],

    [ 'Test PIPE2',
      'echo Good | grep -n G',
      '1:Good' ],

    [ 'Test PIPE3',
      'echo Bad | grep -c G',
      '0' ],

    [ 'Test PIPE4',
      'echo Line | cat | wc -l',
      '1' ],

    [ 'Test PIPE5',
      '../sh61 -q cmd%%.sh; ps | grep sleep | grep -v grep | head -n 1 | wc -l',
      'Hello Bye 1',
      CMD_INIT => 'echo "echo Hello; sleep 2 & echo Bye" > cmd%%.sh'],

    [ 'Test PIPE6',
      "echo GoHangASalamiImALasagnaHog | $rev | $rev | $rev",
      'goHangasaLAmIimalaSAgnaHoG' ],

    [ 'Test PIPE7',
      "$rev f%%.txt | $rev",
      'goHangasaLAmIimalaSAgnaHoG',
      CMD_INIT => 'echo goHangasaLAmIimalaSAgnaHoG > f%%.txt' ],

    [ 'Test PIPE8',
      "cat f%%.txt | tr [A-Z] [a-z] | $CHECKSUM | tr -d -",
      '8e21d03f7955611616bcd2337fe9eac1',
      CMD_INIT => 'echo goHangasaLAmIimalaSAgnaHoG > f%%.txt' ],

    [ 'Test PIPE9',
      "$rev f%%.txt | $CHECKSUM | tr [a-z] [A-Z] | tr -d -",
      '502B109B37EC769342948826736FA063',
      CMD_INIT => 'echo goHangasaLAmIimalaSAgnaHoG > f%%.txt' ],

    [ 'Test PIPE10',
      'sleep 2 & sleep 0.2; ps T | grep sleep | grep -v grep | head -n 1 | wc -l',
      '1',
      CMD_FILE => 1 ],

    [ 'Test PIPE11',
      'echo Sedi | tr d m ; echo Calan | tr a o',
      'Semi Colon' ],

    [ 'Test PIPE12',
      'echo Ignored | echo Desired',
      'Desired' ],

    [ 'Test PIPE13',
      '../sh61 -q cmd%%.sh &',
      'Hello 1',
      CMD_INIT => 'echo "echo Hello; sleep 0.4" > cmd%%.sh',
      CMD_CLEANUP => 'echo "sleep 0.2 ; ps T | grep sleep | grep -v grep | head -n 1 | wc -l" > cmd%%b.sh; ../sh61 -q cmd%%b.sh',
      CMD_CAREFUL_CLEANUP => 1,
      CMD_FILE => 1 ],

    [ 'Test PIPE14',
      'true | true && echo True',
      'True' ],

    [ 'Test PIPE15',
      'true | echo True || echo False',
      'True' ],

    [ 'Test PIPE16',
      'false | echo True || echo False',
      'True' ],

    [ 'Test PIPE17',
      'echo Hello | grep -q X || echo NoXs',
      'NoXs' ],

    [ 'Test PIPE18',
      'echo Yes | grep -q Y && echo Ys',
      'Ys' ],

    [ 'Test PIPE19',
      'echo Hello | grep -q X || echo poqs | tr pq NX',
      'NoXs' ],

    [ 'Test PIPE20',
      'echo Yes | grep -q Y && echo fs | tr f Y',
      'Ys' ],

    [ 'Test PIPE21',
      'false && echo vnexpected | tr v u ; echo expected',
      'expected' ],

    [ 'Test PIPE22',
      'false && echo unexpected && echo vnexpected | tr v u ; echo expected',
      'expected' ],

    [ 'Test PIPE23', # actually a background test
      'sleep 0.2 | wc -c | sed s/0/Second/ & sleep 0.1 | wc -c | sed s/0/First/',
      'First Second',
      CMD_CLEANUP => 'sleep 0.25'],

    [ 'Test PIPE24',
      'yes | head -n 5',
      'y y y y y' ],


# Zombies
    [ 'Test ZOMBIE1',
      "sleep 0.05 &\nsleep 0.1\nps T",
      '',
      CMD_OUTPUT_FILTER => 'grep defunct | grep -v grep'],

    [ 'Test ZOMBIE2',
      "sleep 0.05 & sleep 0.05 & sleep 0.05 & sleep 0.05 &\nsleep 0.07\nsleep 0.07\nps T",
      '',
      CMD_OUTPUT_FILTER => 'grep defunct | grep -v grep'],


# Redirection
    [ 'Test REDIR1',
      'echo Start ; echo File > f%%.txt',
      'Start File',
      CMD_CLEANUP => 'cat f%%.txt'],

    [ 'Test REDIR2',
      'tr pq Fi < f%%.txt ; echo Done',
      'File Done',
      CMD_INIT => 'echo pqle > f%%.txt'],

    [ 'Test REDIR3',
      "perl -e 'print STDERR $$' 2> f%%.txt ; grep '^[1-9]' f%%.txt | wc -l ; rm -f f%%.txt",
      '1',
      CMD_INIT => 'echo File > f%%.txt' ],

    [ 'Test REDIR4',
      "perl -e 'print STDERR $$; print STDOUT \"X\"' > f%%a.txt 2> f%%b.txt ; grep '^[1-9]' f%%a.txt | wc -l ; grep '^[1-9]' f%%b.txt | wc -l ; cmp -s f%%a.txt f%%b.txt || echo Different",
      '0 1 Different',
      CMD_INIT => 'echo File > f%%.txt' ],

    [ 'Test REDIR5',
      'tr hb HB < f%%.txt | sort | ../sh61 -q cmd%%.sh',
      'Bye Hello First Good',
      CMD_INIT => 'echo "head -n 2 ; echo First && echo Good" > cmd%%.sh; (echo hello; echo bye) > f%%.txt'],

    [ 'Test REDIR6',
      'sort < f%%a.txt > f%%b.txt ; tail -n 2 f%%b.txt ; rm -f f%%a.txt f%%b.txt',
      'Bye Hello',
      # (Remember, CMD_INIT is a normal shell command! For your shell,
      # parentheses are extra credit.)
      CMD_INIT => '(echo Hello; echo Bye) > f%%a.txt'],

    [ 'Test REDIR7',
      'echo > /tmp/directorydoesnotexist/foo',
      'No such file or directory',
      CMD_CLEANUP => 'perl -pi -e "s,^.*:\s*,," out%%.txt' ],

    [ 'Test REDIR8',
      'echo > /tmp/directorydoesnotexist/foo && echo Unwanted',
      'No such file or directory',
      CMD_CLEANUP => 'perl -pi -e "s,^.*:\s*,," out%%.txt' ],

    [ 'Test REDIR9',
      'echo > /tmp/directorydoesnotexist/foo || echo Wanted',
      'No such file or directory Wanted',
      CMD_CLEANUP => 'perl -pi -e "s,^.*:\s*,," out%%.txt' ],

    [ 'Test REDIR10',
      'echo Hello < nonexistent%%.txt',
      'No such file or directory',
      CMD_CLEANUP => 'perl -pi -e "s,^.*:\s*,," out%%.txt' ],

    [ 'Test REDIR11',
      'echo Hello < nonexistent%%.txt && echo Unwanted',
      'No such file or directory',
      CMD_CLEANUP => 'perl -pi -e "s,^.*:\s*,," out%%.txt' ],

    [ 'Test REDIR12',
      'echo Hello < nonexistent%%.txt || echo Wanted',
      'No such file or directory Wanted',
      CMD_CLEANUP => 'perl -pi -e "s,^.*:\s*,," out%%.txt' ],

    [ 'Test REDIR13',
      'cat unwanted.txt | cat < wanted.txt',
      'Wanted',
      CMD_INIT => 'echo Unwanted > unwanted.txt; echo Wanted > wanted.txt' ],

    [ 'Test REDIR14',
      'cat < wanted.txt | cat > output.txt',
      'output.txt is Wanted',
      CMD_INIT => 'echo Wanted > wanted.txt',
      CMD_CLEANUP => 'echo output.txt is; cat output.txt' ],

    [ 'Test REDIR15',
      'cat < xoqted.txt | tr xoq Wan | cat > output.txt',
      'output.txt is Wanted',
      CMD_INIT => 'echo xoqted > xoqted.txt',
      CMD_CLEANUP => 'echo output.txt is; cat output.txt' ],

    [ 'Test REDIR16',
      'echo Ignored | cat < lower.txt | tr A-Z a-z',
      'lower',
      CMD_INIT => 'echo LOWER > lower.txt' ],


# cd
    [ 'Test CD1',
      'cd / ; pwd',
      '/' ],

    [ 'Test CD2',
      'cd / ; cd /usr ; pwd',
      '/usr' ],

# cd without redirecting stdout
    [ 'Test CD3',
      'cd / ; cd /doesnotexist 2> /dev/null ; pwd',
      '/' ],

# Fancy conditionals
    [ 'Test CD4',
      'cd / && pwd',
      '/' ],

    [ 'Test CD5',
      'echo go ; cd /doesnotexist 2> /dev/null > /dev/null && pwd',
      'go' ],

    [ 'Test CD6',
      'cd /doesnotexist 2> /dev/null > /dev/null || echo does not exist',
      'does not exist' ],

    [ 'Test CD7',
      'cd /tmp && cd / && pwd',
      '/' ],

    [ 'Test CD8',
      'cd / ; cd /doesnotexist 2> /dev/null > /dev/null ; pwd',
      '/' ],

# Interrupts
    [ 'Test INTR1',
      'echo a && sleep 0.2 && echo b',
      'a',
      CMD_INT_DELAY => 0.1 ],

    [ 'Test INTR2',
      "echo start && sleep 0.2 && echo undesired \n echo end",
      'start end',
      CMD_FILE => 1,
      CMD_INT_DELAY => 0.1 ],

    [ 'Test INTR3',
      'sleep 0.3 && echo yes & sleep 0.2 && echo no',
      'yes',
      CMD_CLEANUP => 'sleep 0.25',
      CMD_INT_DELAY => 0.1 ],

    [ 'Test INTR4',
      'sleep 1',
      '',
      CMD_INT_DELAY => 0.1,
      CMD_MAX_TIME => 0.15 ],

    [ 'Test INTR5',
      '../sh61 -q cmd%%.sh',
      '',
      CMD_INIT => 'echo "sleep 1 && echo undesired" > cmd%%.sh',
      CMD_INT_DELAY => 0.1,
      CMD_MAX_TIME => 0.15 ]

    );

my($ntest) = 0;

my($sh) = "./sh61";
-d "out" || mkdir("out") || die "Cannot create 'out' directory\n";
my($ntestfailed) = 0;

# check for a ton of existing sh61 processes
$me = `id -un`;
chomp $me;
open(SH61, "ps uxww | grep '^$me.*sh61' | grep -v grep |");
$nsh61 = 0;
$nsh61 += 1 while (defined($_ = <SH61>));
close SH61;
if ($nsh61 > 5) {
    print STDERR "\n";
    print STDERR "${Red}**** Looks like $nsh61 ./sh61 processes are already running.\n";
    print STDERR "**** Do you want all those processes?\n";
    print STDERR "**** Run 'killall -9 sh61' to kill them.${Off}\n\n";
}

# remove output files
opendir(DIR, "out") || die "opendir: $!\n";
foreach my $f (grep {/\.(?:txt|sh)$/} readdir(DIR)) {
    unlink("out/$f");
}
closedir(DIR);

if (!-x $sh) {
    $editsh = $sh;
    $editsh =~ s,^\./,,;
    print STDERR "${Red}$sh does not exist, so I can't run any tests!${Off}\n(Try running \"make $editsh\" to create $sh.)\n";
    exit(1);
}

select STDOUT;
$| = 1;

my($testsrun) = 0;
my($testindex) = 0;

sub remove_files ($) {
    my($testnumber) = @_;
    opendir(DIR, "out");
    foreach my $f (grep {/$testnumber\.(?:sh|txt)$/} readdir(DIR)) {
        unlink("out/$f");
    }
    closedir(DIR);
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
    pipe(OR, OW) or die "pipe";
    fcntl(OR, F_SETFL, fcntl(OR, F_GETFL, 0) | O_NONBLOCK);
    1 while waitpid(-1, WNOHANG) > 0;

    open(TTY, "+<", "/dev/tty") or die "can't open /dev/tty: $!";

    $run61_pid = fork();
    if ($run61_pid == 0) {
        POSIX::setpgid(0, 0) or die("child setpgid: $!\n");
        defined($dir) && chdir($dir);

        my($fn) = defined($opt{"stdin"}) ? $opt{"stdin"} : "/dev/null";
        if (defined($fn) && $fn ne "/dev/stdin") {
            my($fd) = POSIX::open($fn, O_RDONLY);
            POSIX::dup2($fd, 0);
            POSIX::close($fd) if $fd != 0;
            fcntl(STDIN, F_SETFD, fcntl(STDIN, F_GETFD, 0) & ~FD_CLOEXEC);
        }

        close(OR);
        if (!defined($outfile) || $outfile ne "/dev/stdout") {
            open(OW, ">", $outfile) || die if defined($outfile) && $outfile ne "pipe";
            POSIX::dup2(fileno(OW), 1);
            POSIX::dup2(fileno(OW), 2);
            close(OW) if fileno(OW) != 1 && fileno(OW) != 2;
            fcntl(STDOUT, F_SETFD, fcntl(STDOUT, F_GETFD, 0) & ~FD_CLOEXEC);
            fcntl(STDERR, F_SETFD, fcntl(STDERR, F_GETFD, 0) & ~FD_CLOEXEC);
        }

        { exec($command) };
        print STDERR "error trying to run $command: $!\n";
        exit(1);
    }

    POSIX::setpgid($run61_pid, $run61_pid) or die("setpgid: $!\n");
    POSIX::tcsetpgrp(fileno(TTY), $run61_pid) or die "tcsetpgrp: $!\n";

    my($before) = Time::HiRes::time();
    my($died) = 0;
    my($time_limit) = exists($opt{"time_limit"}) ? $opt{"time_limit"} : 0;
    my($out, $buf, $nb) = ("", "");
    my($answer) = exists($opt{"answer"}) ? $opt{"answer"} : {};
    $answer->{"command"} = $command;
    my($sigint_at) = defined($opt{"int_delay"}) ? $before + $opt{"int_delay"} : undef;

    close(OW);

    eval {
        do {
            my($delta) = 0.3;
            if ($sigint_at) {
                my($now) = Time::HiRes::time();
                $delta = min($delta, $sigint_at < $now + 0.02 ? 0.1 : $sigint_at - $now);
            }
            Time::HiRes::usleep($delta * 1e6) if $delta > 0;

            if (waitpid($run61_pid, WNOHANG) > 0) {
                $answer->{"status"} = $?;
                die "!";
            }
            if ($sigint_at && Time::HiRes::time() >= $sigint_at) {
                my($pgrp) = POSIX::tcgetpgrp(fileno(TTY));
                unless ($pgrp == getpgrp()) {
                    kill('INT', -$pgrp);
                    $sigint_at = undef;
                }
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
        } while (Time::HiRes::time() < $before + $time_limit);
        $died = sprintf("timeout after %.2fs", $time_limit)
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
        return $answer;
    }

    if (defined($outfile) && $outfile ne "pipe") {
        $out = "";
        close(OR);
        open(OR, "<", (defined($dir) ? "$dir/$outfile" : $outfile));
    }
    $answer->{"output"} = run_sh61_pipe($out, fileno(OR), $size_limit);
    close(OR);

    return $answer;
}

sub test_runnable ($$) {
    my($prefix, $number) = @_;
    $prefix = lc($prefix);
    return !@ALLOW_TESTS || grep {
        $prefix eq $_->[0]
            && ($_->[1] eq ""
                || ($number >= $_->[1]
                    && ($_->[2] eq "-" || $number <= ($_->[2] eq "" ? $_->[1] : -$_->[2]))));
    } @ALLOW_TESTS;
}

sub kill_sleeps () {
    open(PS, "ps T |");
    while (defined($_ = <PS>)) {
        $_ =~ s/^\s+//;
        my(@x) = split(/\s+/, $_);
        if (@x && $x[0] =~ /\A\d+\z/ && $x[4] eq "sleep") {
            kill("INT", $x[0]);
        }
    }
    close(PS);
}

sub disallowed_signal ($) {
    my($s) = @_;
    my(@sigs) = split(" ", $Config{sig_name});
    return "unknown signal $s" if $s >= @sigs;
    return "illegal instruction" if $sigs[$s] eq "ILL";
    return "abort signal" if $sigs[$s] eq "ABRT";
    return "floating point exception" if $sigs[$s] eq "FPE";
    return "segmentation fault" if $sigs[$s] eq "SEGV";
    return "broken pipe" if $sigs[$s] eq "PIPE";
    return "SIG" . $sigs[$s];
}

sub run (@) {
    my($testnumber);
    if ($_[0] =~ /^Test (\w*?)(\d*)(\.\d+|[a-z]+|)(?=$|[.:\s])/i) {
        $testnumber = $1 . $2 . $3;
        return if !test_runnable($1, $2);
    } else {
        $testnumber = "x" . $testindex;
    }

    for (my $i = 0; $i < @_; ++$i) {
        $_[$i] =~ s/\%\%/$testnumber/g;
    }

    my($desc, $in, $want) = @_;
    my(%opts);
    if (@_ > 3 && substr($_[3], 0, 4) ne "CMD_") {
        print STDERR "Warning: old test format ", $desc, "\n";
        $opts{CMD_INIT} = $_[3] if $_[3];
        $opts{CMD_CLEANUP} = $_[4] if $_[4];
    } elsif (@_ > 3) {
        %opts = @_[3..(@_ - 1)];
    }
    return if $opts{CMD_SECRET} && !$ALLOW_SECRET;
    return if !$opts{CMD_SECRET} && !$ALLOW_OPEN;

    $ntest++;
    remove_files($testnumber);
    kill_sleeps();
    system("cd out; " . $opts{CMD_INIT}) if $opts{CMD_INIT};

    print OUT "$desc: ";
    my($tempfile) = "main$testnumber.sh";
    my($outfile) = "out$testnumber.txt";
    open(F, ">out/$tempfile") || die;
    print F $in, "\n";
    close(F);

    my($start) = Time::HiRes::time();
    my($cmd) = "../$sh -q" . ($opts{CMD_FILE} ? " $tempfile" : "");
    my($stdin) = $opts{CMD_FILE} ? "/dev/stdin" : $tempfile;
    my($info) = run_sh61($cmd, "stdin" => $stdin, "stdout" => $outfile, "time_limit" => 10, "size_limit" => 1000, "dir" => "out", "nokill" => 1, "delay" => 0.05, "int_delay" => $opts{CMD_INT_DELAY});

    if ($opts{CMD_CLEANUP}) {
        if ($opts{CMD_CAREFUL_CLEANUP}) {
            my($infox) = run_sh61("{ " . $opts{CMD_CLEANUP} . "; } >>$outfile 2>&1", "time_limit" => 5, "dir" => "out", "stdin" => "/dev/stdin", "stdout" => "/dev/stdout");
            $info->{killed} = "cleanup command killed"
                if exists($infox->{killed}) && !exists($info->{killed});
        } else {
            system("cd out; { " . $opts{CMD_CLEANUP} . "; } >>$outfile 2>&1");
        }
    }
    system("cd out; mv $outfile ${outfile}~; { " . $opts{CMD_OUTPUT_FILTER} . "; } <${outfile}~ >$outfile 2>&1")
        if $opts{CMD_OUTPUT_FILTER};

    kill 9, -$info->{"pgrp"} if exists($info->{"pgrp"});

    my($ok, $prefix, $sigdead) = (1, "");
    if (exists($info->{"status"})
        && ($info->{"status"} & 127)  # died from signal
        && ($sigdead = disallowed_signal($info->{"status"} & 127))) {
        print OUT "${Red}KILLED${Redctx} by $sigdead${Off}\n";
        $ntestfailed += 1 if $ok;
        $ok = 0;
        $prefix = "  ";
    }
    $result = `cat out/$outfile`;
    # sanitization errors
    my($sanitizera, $sanitizerb) = ("", "");
    if ($result =~ /\A([\s\S]*?)^(===+\s+==\d+==\s*ERROR[\s\S]*)\z/m) {
        $result = $1;
        $sanitizerb = $2;
    }
    while ($result =~ /\A([\s\S]*?)^(\S+\.cc:\d+:(?:\d+:)? runtime error.*(?:\n|\z)|=+\s+WARNING.*Sanitizer[\s\S]*?\n=+\n)([\s\S]*)\z/m) {
        $result = $1 . $3;
        $sanitizera .= $2;
    }
    my($sanitizer) = $sanitizera . $sanitizerb;
    $result =~ s%^sh61[\[\]\d]*\$ %%m;
    $result =~ s%sh61[\[\]\d]*\$ $%%m;
    $result =~ s%^\[\d+\]\s+\d+$%%mg;
    $result =~ s|\[\d+\]||g;
    $result =~ s|^\s+||g;
    $result =~ s|\s+| |g;
    $result =~ s|\s+$||;
    if (($result eq $want
         || ($want eq 'Syntax error [NULL]' && $result eq '[NULL]'))
        && !exists($info->{killed})
        && (!$opts{CMD_MAX_TIME} || $info->{time} <= $opts{CMD_MAX_TIME})) {
        # remove all files unless @ARGV was set
        print OUT "${Green}passed${Off}\n" if $ok;
        remove_files($testnumber) if !@ARGV && $ok;
    } else {
        printf OUT "$prefix${Red}FAILED${Redctx} in %.3f sec${Off}\n", $info->{time};
        $in =~ s/\n/ \\n /g;
        print OUT "    command  \`$in\`\n";
        if ($result eq $want) {
            print OUT "    output   \`$want\`\n" if $want ne "";
        } else {
            print OUT "    expected \`$want\`\n";
            $result = substr($result, 0, 76) . "..." if length($result) > 76;
            print OUT "    got      \`$result\`\n";
        }
        if ($opts{CMD_MAX_TIME} && $info->{time} > $opts{CMD_MAX_TIME}) {
            printf OUT "    should have completed in %.3f sec\n", $opts{CMD_MAX_TIME};
        }
        if (exists($info->{killed})) {
            print OUT "  ", $info->{killed}, "\n";
        }
        $ntestfailed += 1 if $ok;
    }
    if ($sanitizer ne "") {
        chomp $sanitizer;
        $sanitizer = substr($sanitizer, 0, 1200) . "..."
            if length($sanitizer) > 1200;
        $sanitizer =~ s/\n/\n      /g;
        print OUT "    ${Redctx}sanitizer reports errors:${Off}\n      $sanitizer\n";
    }

    if (exists($opts{CMD_CAT})) {
        print OUT "\n${Green}", $opts{CMD_CAT}, "\n==================${Off}\n";
        if (open(F, "<", "out/" . $opts{CMD_CAT})) {
            print OUT $_ while (defined($_ = <F>));
            close(F);
        } else {
            print OUT "${Red}out/", $opts{CMD_CAT}, ": $!${Off}\n";
        }
        print OUT "\n";
    }
}

open(OUT, ">&STDOUT");
my($leak_check) = 0;

while (@ARGV && $ARGV[0] =~ /^-/) {
    if ($ARGV[0] eq "--leak" || $ARGV[0] eq "--leak-check") {
        $leak_check = 1;
        shift @ARGV;
        next;
    } elsif ($ARGV[0] =~ /\A--leak=(.*)\z/) {
        $leak_check = ($1 eq "1" || $1 eq "yes");
        shift @ARGV;
        next;
    } elsif ($ARGV[0] eq "--ignore-sigint") {
        $SIG{"INT"} = "IGNORE";
        shift @ARGV;
        next;
    } elsif ($ARGV[0] eq "--only") {
        @ARGV = ($ARGV[1]);
        last;
    }

    print STDERR "Usage: check.pl TESTNUM...\n";
    exit(1);
}

if (!$leak_check && !$ENV{"ASAN_OPTIONS"}) {
    $ENV{"ASAN_OPTIONS"} = "detect_leaks=0";
}

foreach my $allowed_tests (@ARGV) {
    while ($allowed_tests =~ m{(?:^|[\s,])(\w+?)-?(\d*)((?:-\d*)?)(?=[\s,]|$)}g) {
        push(@ALLOW_TESTS, [lc($1), $2, $3]);
    }
}

foreach $test (@tests) {
    ++$testindex;
    run(@$test);
}

my($ntestpassed) = $ntest - $ntestfailed;
print OUT "\r$ntestpassed of $ntest ", ($ntest == 1 ? "test" : "tests"), " passed\n" if $ntest > 1;
exit(0);
