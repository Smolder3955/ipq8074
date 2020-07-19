#!/usr/bin/perl
use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin";
use Parser::Config;
use Parser::Layout;

my %CMDLINE_OPTS;

my %COMMANDS = (
    nor => {
        init => "sf probe",
        erase => "sf erase %s +%s",
	write => "sf write \$fileaddr %s %s",
    },
    nand => {
        set_layout => "ipq_nand %s",
        erase => "nand erase %s %s",
        write => "nand write \$fileaddr %s %s",
    },
);

sub write_script($$$) {
    my $fh = shift;
    my $layout = shift;
    my $cfg = shift;

    my $flash_type = $cfg->{flash_type};

    printf $fh ("set imgaddr \${fileaddr}\n");
    printf $fh ($COMMANDS{$flash_type}->{init}."\n")
            if exists($COMMANDS{$flash_type}->{init});
    printf $fh ("\n");

    foreach ($cfg->get_partitions_list()) {
        my $offset = sprintf ("0x%08x",$layout->get_offset($_));
        my $size = $layout->is_fixed_size($_)
                   ? sprintf ("0x%08x", $layout->get_size($_))
                   : '$filesize';

        printf $fh ("# $_\n");

        printf $fh ($COMMANDS{$flash_type}->{set_layout} . "\n",
                    $cfg->get_layout($_))
                if exists($COMMANDS{$flash_type}->{set_layout});

        printf $fh ("imxtract \$imgaddr $_ || exit 1\n");

        printf $fh ($COMMANDS{$flash_type}->{erase} . " || exit 1\n",
                    $offset, $size);
        printf $fh ($COMMANDS{$flash_type}->{write} . " || exit 1\n",
                    $offset, $size);

        printf $fh ("\n");
    }

    printf $fh ("echo \"Flashing complete with success\"\n");
}

sub process_config() {
    my $cfg = Parser::Config->new($CMDLINE_OPTS{config});
    my $out = exists($CMDLINE_OPTS{out}) ?
                $CMDLINE_OPTS{out} : $cfg->get_description().".scr";

    my $layout = Parser::Layout->new($CMDLINE_OPTS{partdir}."/"
                                     .$cfg->{partition_layout});
    $layout->set_sector_size($cfg->{sector_size});
    $layout->set_flash_size($cfg->{flash_size});
    open (my $fh, '>', $out) or die ("Can't open out.scr\n");
    write_script($fh, $layout, $cfg);
    close($fh);
}

sub parse_options() {
    while (my $arg = shift @ARGV) {
        $arg =~ /-c/ and $CMDLINE_OPTS{config} = shift @ARGV;
        $arg =~ /-p/ and $CMDLINE_OPTS{partdir} = shift @ARGV;
        $arg =~ /-o/ and $CMDLINE_OPTS{out} = shift @ARGV;
    }

    exists($CMDLINE_OPTS{partdir}) or $CMDLINE_OPTS{partdir} = ".";

    exists($CMDLINE_OPTS{config})
        or die "Please specify a config file using \"-c config.xml\"";
}

parse_options();
process_config();
