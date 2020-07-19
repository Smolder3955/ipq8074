package Parser::Config;
use strict;
use warnings;
use XML::Simple qw(:strict);

sub new($) {
    my $class = shift;
    my $file = shift;

    my $ref = XMLin($file,
                    ForceArray => [ 'partition' ],
                    KeyAttr => { });

    $ref->{filename} = $file;

    return bless $ref, $class;
}

###############################################################################
# Private functions                                                           #
###############################################################################

sub _get_part_id($) {
    my $self = shift;
    my $name = shift;

    my $cur_id = 0;
    foreach(@{$self->{partition}}) {
        if ($name eq $self->{partition}[$cur_id]->{name}) {
            return $cur_id;
        }
        $cur_id++;
    }
    die "Partition \"$name\" not found in $self->{filename}\n";
    return undef;
}

###############################################################################
# Public functions                                                            #
###############################################################################

sub get_description() {
    my $self = shift;
    my $name = $self->{desc};
    $name =~ s/ /-/g;
    return lc $name;
}

sub get_partitions_list() {
    my $self = shift;
    my @list;
    foreach (@{$self->{partition}}) {
        push (@list, $_->{name});
    }
    return @list;
}

sub get_layout() {
    my $self = shift;
    my $name = shift;

    return $self->{partition}[$self->_get_part_id($name)]->{layout};
}

sub get_filename($) {
    my $self = shift;
    my $name = shift;

    return $self->{partition}[$self->_get_part_id($name)]->{file};
}
sub is_boot($) {
    my $self = shift;
    my $name = shift;
    return exists($self->{partition}[$self->_get_part_id($name)]->{boot});
}

1;
