#!/usr/bin/perl
use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin";
use Parser::Config;
use Parser::Layout;

my %CMDLINE_OPTS;

my $DTS_TEMPLATE = << 'EOF'
/dts-v1/;

/ {
	description = "%s";

	images {
%s
	};
};
EOF
;

my $IMAGE_TEMPLATE = << 'EOF'
		%s {
			description = "%s";
			data = /incbin/("%s");
			type = "%s";
			arch = "arm";
			compression = "none";

			hash@1 {
				algo = "crc32";
			};
		};
EOF
;

sub image($$$) {
    my $name = shift;
    my $file = shift;
    my $type = shift;

    return sprintf ($IMAGE_TEMPLATE, $name, $name, $file, $type);
}

sub dts($) {
    my $cfg = shift;

    my $images = image("SCRIPT", $CMDLINE_OPTS{script}, "script")
        if (exists($CMDLINE_OPTS{script}));

    foreach ($cfg->get_partitions_list()) {
        $images .= image($_, $CMDLINE_OPTS{imagedir}."/".$cfg->get_filename($_),
                         "firmware");
    }
    return sprintf($DTS_TEMPLATE, $cfg->get_description(), $images);
}

sub process_config() {
    my $cfg = Parser::Config->new($CMDLINE_OPTS{config});
    my $out = exists($CMDLINE_OPTS{out}) ?
                $CMDLINE_OPTS{out} : $cfg->get_description() . ".its";

    open (my $fh, '>', $out) or die ("Can't open out.scr\n");
    printf $fh (dts($cfg));
    close($fh);
}

sub parse_options() {
    while (my $arg = shift @ARGV) {
        $arg =~ /-s/ and $CMDLINE_OPTS{script} = shift @ARGV;
        $arg =~ /-c/ and $CMDLINE_OPTS{config} = shift @ARGV;
        $arg =~ /-i/ and $CMDLINE_OPTS{imagedir} = shift @ARGV;
        $arg =~ /-o/ and $CMDLINE_OPTS{out} = shift @ARGV;
    }

    exists($CMDLINE_OPTS{imagedir}) or $CMDLINE_OPTS{imagedir} = ".";

    exists($CMDLINE_OPTS{config})
        or die "Please specify a config file using \"-c config.xml\"";
}

parse_options();
process_config();
