#!/usr/bin/perl -W
# Stupid doc scanner
# Copyright (C) 2001 Tim Janik
#
# This code is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
use strict;
use POSIX "strftime";

my $pname = "Foo-Manual";
my $pblurb = "Official Foo Manual";
my $package = "FooFactory";

my @seealso = ();

# parse options
while ($_ = $ARGV[0], defined $_ && /^-/) {
    shift;
    last if /^--$/;
    if (/^--name$/) { $pname = shift; }
    elsif (/^--blurb$/) { $pblurb = shift; }
    elsif (/^--package$/) { $package = shift; }
    elsif (/^--seealso$/) { push @seealso, shift; }
}

# docs
my @records = ();
my %declhash = ();

while (<>) {
    my $type = 0;
    my $file_name = $ARGV;

    # reset line numbering
    close (ARGV) if (eof);

    # read lines until comment end is matched
    while (m@/\*([^*]|\*[^/*])*\**$@x) {
        my $new = <>;
        (defined ($new) && ($file_name eq $ARGV)) or die "$file_name:$.: Unmatched comment\n";
        $_ .= $new;
    }

    # read lines until function decl is complete
    while (m@^\w[^(]*\([^{;]*$@x) {
        my $new = <>;
        (defined ($new) && ($file_name eq $ARGV)) or die "$file_name:$.: Unmatched function declaration\n";
        $_ .= $new;
    }

    # match docu comment
    # if (m@^/\*\*\s+(([^*]|\*[^/*]|\*\*[^/])*)\*\*/@) {
    if (m@^/\*\*\s+(([^*]|\*+[^*/])*)\*+/@) {
	my @lines = split ('\n', $1);
	my $line;
	my $rec = { name => 'Unnamed',
		    var_names => [], var_blurbs => [], var_types => [],
		    var_hash => {}, found_decl => 0,
		    text => '', returns => [] };
	my $first_line = 1;

	# print STDERR "found doc comment in $file_name:$.:\n";
	for my $line (@lines) {
	    $line =~ s/^\s*\**\s*//;
	    if ($first_line) {
		$line =~ s/^\s*//;
		$line =~ s/\s*$//;
		$line =~ s/\+/::/;
		$rec->{name} = $line;
		$declhash{$line} = $rec;
	    } elsif ($line =~ m/@([A-Za-z._][A-Za-z0-9._-]*)\s*:\s*(.*)\s*$/) {
		my $name = $1;
		my $blurb = $2;

		if ($name =~ m/^returns$/i) {
		    my $returns = $rec->{returns};
		    push @$returns, $blurb;
		} else {
		    my $names = $rec->{var_names};
		    my $blurbs = $rec->{var_blurbs};
		    my $var_hash = $rec->{var_hash};
		    $var_hash->{$name} = $blurb;
		    push @$names, $name;
		    push @$blurbs, $blurb;
		}
	    } else {
		$line =~ s/^\s*//;
		$line =~ s/\s*$//;
		$rec->{text} .= $line;
		$rec->{text} .= ' ' if (length ($rec->{text}));
	    }
	    $first_line = 0;
	}
	push @records, $rec;
    }


    # try to match function decls that we know about
    # if (m@([A-Za-z._][A-Za-z0-9._-]*)\s*\(([A-Za-z0-9\s,*_-]*)\)\s*\{@) {
    if (m@([A-Za-z._][A-Za-z0-9._-]*)\s*\(([A-Za-z0-9\s,*_-]*)\)\s*[{]@) {
	my $name = $1;
	my $declargs = $2;
	my $rec = $declhash{$name};

        if (defined $rec) {  # have docu record for this decl
	    # print STDERR "ARGDECL: $name: $declargs\n";
	    my @args = split (',', $declargs);
	    $rec->{var_names} = [];
	    $rec->{var_types} = [];
	    $rec->{var_blurbs} = [];
	    $rec->{found_decl} = 1;
	    for my $arg (@args) {
		$arg =~ s/^\s*//;
		$arg =~ s/\s*$//;
		my $t = $arg;

		$t =~ s/^(.*\W)\w*$/$1/;
		$arg = substr ($arg, length $t);
		$t =~ s/\s*\*\s*/*/g;
		$t =~ s/\s\s/ /g;
		# print STDERR "found: $name: $t -- $arg\n";
		if (length $arg) {   # for $t=="void", $arg will be ""
		    my $names = $rec->{var_names};
		    my $types = $rec->{var_types};
		    my $blurbs = $rec->{var_blurbs};
		    my $hash = $rec->{var_hash};
		    my $blurb = $hash->{$arg};
		    if (defined $blurb) {
			delete $hash->{$arg};
		    } else {
			$blurb = '';
			print STDERR "NOTE: missing description for \`$name(ARG: $arg)'\n";
		    }
		    push @$names, $arg;
		    push @$types, $t;
		    push @$blurbs, $blurb;
		}
	    }
	}
    }
}

sub tags_print_syntax {
    my $rec       = shift;
    my $prefix    = shift || '';
    my @var_names = @{$rec->{var_names}};

    print $prefix, '@reference_function{' . $rec->{name} . '} (',
	join(', ', map { $_ = "\@reference_parameter{$_}" } @var_names),
	");\n";
}
sub tags_highlight {
    my $t = shift;
    $t =~ s/@([A-Za-z0-9_-]+)/\@emph{$1}/g;
    $t =~ s/%([A-Za-z0-9_-]+)/\@emph{$1}/g;
    $t =~ s/#([A-Za-z0-9_-]+)/\@strong{$1}/g;
    # $t =~ s/([A-Za-z0-9_-]+\([A-Za-z0-9\s,*_-]*\))/<strong>$1<\/strong>/g;
    $t =~ s/([A-Za-z0-9_-]+\([+\/%&|^~!A-Za-z0-9\s,*_-]*\))/\@strong{$1}/g;
    return $t;
}
sub tags_print_description {
    my $rec = shift;
    my $var_types = $rec->{var_types};
    my $var_names = $rec->{var_names};
    my $var_blurbs = $rec->{var_blurbs};
    my $returns = $rec->{returns};

    if (@{$var_names} or @{$returns}) {
	print "\n\@multitable \@columnfractions .3 .3 .3\n";

	for (my $i = 0; $i <= $#$var_names; $i++) {
	    my $t = $$var_types[$i] || "";
	    $t =~ s/\t/ /g;
	    # $t =~ s/\s/\\ /g;

	    print "\@item\n";
	    print "\@reference_type{$t}\n" if $t;

	    print "\@tab\n";
	    printf ("\@reference_parameter{%s}\n", $$var_names[$i]) if $$var_names[$i];

	    printf ("\@tab\n%s\n", tags_highlight ($$var_blurbs[$i]));
	}

	for my $r (@$returns) {
	    printf ("\@item\n\@reference_returns{RETURNS:}\n\@tab\n\@tab\n%s\n", tags_highlight ($r));
	}

	print "\@end multitable\n\n";
    } else {
	print "\n";
    }

    print tags_highlight($rec->{text}) . "\n\n";
}

my %test_hash = ();
my @dups = ();

my $tname = lc($pname);
$tname =~ tr/A-Za-z0-9-/_/c;

# \@settitle $pname - $pblurb - $package

print <<END_HEADER;
\\input texinfo
\@c %**start of header
\@settitle $pname
\@footnotestyle end
\@c %**end of header

\@include teximacros.texi

\@docpackage{$package}
\@docfont{tech}

\@unnumbered NAME
\@reference_docname{$pname - $pblurb}
\@*
\@revision{}

\@unnumbered SYNOPSIS
\@printplainindex cp

\@unnumbered DESCRIPTION
END_HEADER

for my $rec (@records) {
    tags_print_syntax($rec, '@cpindex ');
    tags_print_syntax($rec, '@reference_title{}');
    tags_print_description ($rec);

    push (@dups, $rec->{name}) if (defined $test_hash{$rec->{name}});
    $test_hash{$rec->{name}} = 1;
}

# Link to external documents
if (@seealso) {
    print "\@unnumbered SEE ALSO\n";
    print join(', ', map { $_ = "\@uref{$_}" } @seealso);
}

print <<FOOTER;

\@*
\@revision{}

FOOTER

# provide feedback
for my $rec (@records) {
    my $var_hash = $rec->{var_hash};
    next if (!$rec->{found_decl});
    for my $a (keys %$var_hash) {
	print STDERR "NOTE: couldn't find declaration for \`".$rec->{name}."($a)'\n";
    }
}
for (@dups) {
    print STDERR "WARNING: duplicate description for \`$_'\n";
}

sub array_find_str {
    my $a = shift;
    my $s = shift;
    my $i;
    for ($i = 0; $i <= $#$a; $i++) {
	if ($$a[$i] eq $s) {
	    return $i;
	}
    }
    return -1;
}

# vim: ts=8 sw=4
