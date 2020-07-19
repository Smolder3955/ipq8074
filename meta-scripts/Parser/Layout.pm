package Parser::Layout;
use strict;
use warnings;
use XML::Simple qw(:strict);

sub new($) {
    my $class = shift;
    my $file = shift;

    my $ref = XMLin($file,
                    GroupTags => { partitions => 'partition' },
                    ForceArray => [ 'partition' ],
                    KeyAttr => { });

    $ref->{filename} = $file;

    return bless $ref, $class;
}

###############################################################################
# Private functions                                                           #
###############################################################################

sub _check_sector_size() {
    my $self = shift;
    die "Error: Sector size is not defined"
        if (!exists($self->{sector_size}));
}

sub _check_flash_size() {
    my $self = shift;
    die "Error: Flash size is not defined"
        if (!exists($self->{flash_size}));
}

sub _get_part_id($) {
    my $self = shift;
    my $name = shift;

    for (my $id = 0; $id < @{$self->{partitions}}; $id++) {
        # We want to match on both 0:PARTNAME and just PARTNAME as it's likely
        # that the 0: will be omitted, as it doesn't work in the DTS
        if ($self->{partitions}[$id]->{name}->{content} =~ /(\d:)?$name/) {
            return $id;
        }
    }
    die "Partition \"$name\" not found in $self->{filename}\n";
}

sub _get_size($) {
    my $self = shift;
    my $part_id = shift;
    my $part_ref = $self->{partitions}[$part_id];

    my $size = 0;

    # Base size calculation
    if (exists($part_ref->{size_block})) {
        $self->_check_sector_size();
        $size = $part_ref->{size_block}->{content} * $self->{sector_size};
    } elsif (exists($part_ref->{size_kb})) {
        if ($self->_is_fixed_size($part_id)) {
            $size = $part_ref->{size_kb}->{content} * 1024;
        } else {
            $self->_check_flash_size();
            $size = $self->{flash_size} - $self->_get_offset($part_id);
        }
    }

    # Padding calculation
    if (exists($part_ref->{pad_block})) {
        $self->_check_sector_size();
        $size += $part_ref->{pad_block}->{content} * $self->{sector_size};
    } elsif (exists($part_ref->{pad_kb})) {
        # if size is set to 0xFFFFFFFF, it was already taken care of above,
        # so we'll just ignore this case here
        if ($self->_is_fixed_size($part_id)) {
            $size += $part_ref->{pad_kb}->{content} * 1024;
        }
    }

    return $self->_round_up($size);
}

sub _get_offset($) {
    my $self = shift;
    my $id = shift;

    return 0 if ($id == 0);

    return $self->_get_offset($id - 1) + $self->_get_size($id - 1);
}

sub _round_up($) {
    my $self = shift;
    my $size = shift;

    $self->_check_sector_size();
    my $nr_sectors = int((($size - 1) / $self->{sector_size})) + 1;
    return $nr_sectors * $self->{sector_size};
}

sub _is_fixed_size($) {
    my $self = shift;
    my $id = shift;
    my $part_ref = $self->{partitions}[$id];
    return 1 if !exists($part_ref->{size_kb});
    return ($part_ref->{size_kb}->{content} ne "0xFFFFFFFF");
}

###############################################################################
# Public functions                                                            #
###############################################################################

sub set_sector_size($) {
    my $self = shift;
    my $size = shift;

    $size =~ s/(\d+)[kK]/$1/ and $size *= 1024;
    $size =~ s/(\d+)M/$1/ and $size *= 1024*1024;
    $self->{sector_size} = $size;
}

sub set_flash_size($) {
    my $self = shift;
    my $size = shift;

    $size =~ s/(\d+)[kK]/$1/ and $size *= 1024;
    $size =~ s/(\d+)M/$1/ and $size *= 1024*1024;
    $self->{flash_size} = $size;
}

sub exists($) {
    my $self = shift;
    my $name = shift;

    return 1 if defined($self->_get_part_id($name));
    return 0;
}

sub is_fixed_size($) {
    my $self = shift;
    my $name = shift;
    return $self->_is_fixed_size($self->_get_part_id($name));
}

sub get_size($) {
    my $self = shift;
    my $name = shift;
    return $self->_get_size($self->_get_part_id($name));
}

sub get_offset($) {
    my $self = shift;
    my $name = shift;
    my $id = $self->_get_part_id($name);
    return $self->_get_offset($id);
}

1;
