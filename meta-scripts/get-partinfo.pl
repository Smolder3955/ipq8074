#!/usr/bin/perl
use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin";
use Parser::Config;
use Parser::Layout;

my %CMDLINE_OPTS;

sub list_partitions() {
    my $cfg = Parser::Config->new($CMDLINE_OPTS{config});
    my $layout = Parser::Layout->new($CMDLINE_OPTS{partdir}."/"
                                     .$cfg->{partition_layout});
    $layout->set_sector_size($cfg->{sector_size});
    $layout->set_flash_size($cfg->{flash_size});

    foreach ($cfg->get_partitions_list()) {
        my $offset = sprintf ("0x%08x",$layout->get_offset($_));
        my $size = sprintf ("0x%08x", $layout->get_size($_));

        printf ("%-20s %s %s\n", $_, $offset, $size);
    }
}

sub parse_options() {
    while (my $arg = shift @ARGV) {
        $arg =~ /-c/ and $CMDLINE_OPTS{config} = shift @ARGV;
        $arg =~ /-p/ and $CMDLINE_OPTS{partdir} = shift @ARGV;
    }

    exists($CMDLINE_OPTS{partdir}) or $CMDLINE_OPTS{partdir} = ".";

    exists($CMDLINE_OPTS{config})
        or die "Please specify a config file using \"-c config.xml\"";
}

parse_options();
list_partitions();
