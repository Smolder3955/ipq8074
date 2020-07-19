#!/usr/bin/perl
use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin";
use Parser::Config;
use Parser::Layout;

my %CMDLINE_OPTS;

sub pad($$) {
    my $file_name = shift;
    my $pad_size = shift;

    if (exists($CMDLINE_OPTS{debug})) {
        printf("Pad %s (0x%x)\n", $file_name, $pad_size);
    }

    # If $pad_size is null, we have nothing to pad. So we'll return right away.
    return if ($pad_size == 0);

    open(FH,">>",$file_name) || die "Can't open file $file_name\n";
    binmode(FH);

    # Position at the end of the file
    seek(FH, 0, 2);
    my $buf = "";
    while ($pad_size--) {
        $buf .= sprintf("\xff");
    }
    print FH $buf;
    close(FH);
}

sub dd($$$$$) {
    my $inf = shift;
    my $outf = shift;
    my $seek = shift;
    my $size = shift;
    my $bs = shift;

    # If the output file is smaller than the seek offset, we must pad it
    if (-e $outf and -s $outf < $seek) {
        pad($outf, $seek - -s $outf);
    }

    if (exists($CMDLINE_OPTS{debug})) {
        printf("%s --> %s (offset=0x%x size=0x%x)\n",
               $inf, $outf, $seek, -s $inf);
    }

    open(INF, "<", $inf) || die "Can't open file $inf\n";
    binmode(INF);
    open(OUTF, ">>", $outf) || die "Can't open file $outf\n";
    binmode(OUTF);

    while (<INF>) {
        print OUTF $_;
    }

    close(INF);
    close(OUTF);

    # Pad until the end of $size
    pad($outf, $size - -s $inf);
}

sub process_config() {
    my $cfg = Parser::Config->new($CMDLINE_OPTS{config});
    my $out = exists($CMDLINE_OPTS{out}) ?
                $CMDLINE_OPTS{out} : $cfg->get_description()."-bootblob.bin";

    # If the output file exists, start by deleting it
    unlink $out;

    my $layout = Parser::Layout->new($CMDLINE_OPTS{partdir}."/"
                                     .$cfg->{partition_layout});
    $layout->set_sector_size($cfg->{sector_size});
    $layout->set_flash_size($cfg->{flash_size});
    for my $part ($cfg->get_partitions_list()) {
        # if it's not a boot partition, no need to write it
        next if (!$cfg->is_boot($part));

        my $in = $cfg->get_filename($part);
        my $offset = $layout->get_offset($part);
        my $size = $layout->get_size($part);

        dd($CMDLINE_OPTS{imagedir}."/".$in, $out, $offset, $size,
           $layout->{sector_size});
    }
}

sub parse_options() {
    while (my $arg = shift @ARGV) {
        $arg =~ /-p/ and $CMDLINE_OPTS{partdir} = shift @ARGV;
        $arg =~ /-i/ and $CMDLINE_OPTS{imagedir} = shift @ARGV;
        $arg =~ /-c/ and $CMDLINE_OPTS{config} = shift @ARGV;
        $arg =~ /-o/ and $CMDLINE_OPTS{out} = shift @ARGV;
        $arg =~ /-d/ and $CMDLINE_OPTS{debug} = 1;
    }

    exists($CMDLINE_OPTS{partdir}) or $CMDLINE_OPTS{partdir} = ".";
    exists($CMDLINE_OPTS{imagedir}) or $CMDLINE_OPTS{imagedir} = ".";

    exists($CMDLINE_OPTS{config})
        or die "Please specify a config file using \"-c config.xml\"";
}

parse_options();
process_config();
