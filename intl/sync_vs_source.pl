#!/usr/bin/perl -W

# $Id: sync_vs_source.pl,v 1.1 2002/12/16 15:06:18 zas Exp $

my %h = ();
my %t = ();

while (<>) {
	while (/\W(T_\w+)\W+/) {
		if ($1 ne "T__N_TEXTS") {
			# found in sources
			$h{$1} = "1";
			$t{$1} = "NULL,";
		}
		$_=$';
	}
}

my @order = ();

open(LNG, "< english.lng") or die "can't open english.lng: $!";
while (<LNG>) {
	if (/^\s*(#*\??)(T_[0-9a-zA-Z_]+),\s*(.+)\s*$/) {
		my $com = $1;
		my $k = $2;
		my $tr = $3;
		push @order, $k;

		#if found in sources and in english.lng
		if (defined($h{$k})) {
			if ($com =~ /^##+/) {
				$h{$k} = $com;
			} else {
				$h{$k} = ""; # uncomment, used in sources and exist
			}
		} else {
			$h{$k} = "#"; # comment

			if ($com =~ /^##+/) {
			 	$h{$k} = "###?";
			}
		}
		$t{$k} = $tr;
	}
}

close(LNG);

for (@order) {
	print $h{$_}, $_, ", ", $t{$_}, "\n";
}

while (($k,$v) = each(%h)) {
	if ($v eq "1") {
		print "##$k, ",$t{$k}?$t{$k}:", NULL,","\n";
	}
}

