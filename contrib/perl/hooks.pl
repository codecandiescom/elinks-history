#!/usr/bin/perl -w
use strict;

################################################################################
### goto_url_hook ##############################################################
sub goto_url_hook
{
	my $url = shift;
	my $current_url = shift;

	# "bugmenot" dumb prefix
	if ($url =~ '^bugmenot$' && $current_url)
	{
		(undef, $current_url) = $current_url =~ /^(.*):\/\/(.*)/;
		$url = 'http://bugmenot.com/view.php?url=' . $current_url;
		return $url;
	}

	# random URL generator
	if ($url =~ '^bored$' || $url =~ '^random$')
	{
		my $word; # you can say *that* again...
		srand();
		open FILE, '</usr/share/dict/words'
			or open FILE, '</usr/share/dict/linux.words'
			or open FILE, '</usr/dict/words'
			or open FILE, '</usr/dict/linux.words'
			or open FILE, '</usr/share/dict/yawl.list'
			or open FILE, $ENV{"HOME"} . '/.elinks/elinks.words'
			or return 'http://google.com/webhp?hl=xx-bork';
		rand($.) < 1 && ($word = $_) while <FILE>;
		close FILE;
		($word) = $word =~ /(.*)/;
		$url = 'http://' . lc($word) . '.com';
		return $url;
	}

	# Google search smart prefix
	if ($url =~ '^(g|google|search|find)(| .*)$')
	{
		my ($search) = $url =~ /^[a-z]* (.*)/;
		if ($search)
		{
			$url = 'http://google.com/search?q=' . $search . '&hl=xx-bork';
		}
		else
		{
			$url = 'http://google.com/webhp?hl=xx-bork';
		}
		return $url;
	}

	# Google Groups smart prefix
	if ($url =~ '^(gg|groups|gr|news|usenet)(| .*)$')
	{
		my ($search) = $url =~ /^[a-z]* (.*)/;
		if ($search)
		{
			$url = 'http://google.com/groups?q=' . $search . '&hl=xx-bork';
		}
		else
		{
			$url = 'http://google.com/groups?hl=xx-bork';
		}
		return $url;
	}

	# MirrorDot smart prefix
	if ($url =~ '^(mirrordot|md)(| .*)$')
	{
		my ($slashdotted) = $url =~ /^[a-z]* (.*)/;
		if ($slashdotted)
		{
			$url = 'http://mirrordot.com/find-mirror.html?' . $slashdotted;
		}
		else
		{
			$url = 'http://mirrordot.com';
		}
		return $url;
	}

	# /. dumb prefix
	if ($url =~ '^\/\.$')
	{
		$url = 'http://slashdot.org';
		return $url;
	}

	# xiferp trams elgooG
	if ($url =~ '^(eg|elgoog|hcraes|dnif)(| .*)$')
	{
		my ($hcraes) = $url =~ /^[a-z]* (.*)/;
		if ($hcraes)
		{
			$url = 'http://alltooflat.com/geeky/elgoog/m/index.cgi?page=%2fsearch&cgi=get&q=' . $hcraes;
		}
		else
		{
			$url = 'http://alltooflat.com/geeky/elgoog/m/index.cgi';
		}
		return $url;
	}

	# the Bastard Operator from Hell
	if ($url =~ '^bofh$')
	{
		$url = 'http://prime-mover.cc.waikato.ac.nz/Bastard.html';
		return $url;
	}

	# torrent search smart prefix
	if ($url =~ '^(bittorrent|torrent|bt)( .*)$')
	{
		my ($torrent) = $url =~ /^[a-z]* (.*)/;
		$url = 'http://google.com/search?q=filetype:torrent ' . $torrent . '&hl=xx-bork';
		return $url;
	}

	# Coral cache smart prefix
	if ($url =~ '^(coral|cc)( .*)$')
	{
		my ($cache) = $url =~ /^[a-z]* (.*)/;
		$cache =~ s/^http:\/\///;
		($url) = $cache =~ s/\//.nyud.net:8090\//;
		$url = 'http://' . $cache;
		return $url;
	}

	# Babelfish smart prefix ("babelfish german english")
	if ($url =~ '^(babelfish|babel|bf|translate|trans)( [a-zA-Z]* [a-zA-Z]*)$' && $current_url)
	{
		$url =~ s/ chinese/ zt/i;
		$url =~ s/ dutch/ nl/i;
		$url =~ s/ english/ en/i;
		$url =~ s/ french/ fr/i;
		$url =~ s/ german/ de/i;
		$url =~ s/ greek/ el/i;
		$url =~ s/ italian/ it/i;
		$url =~ s/ japanese/ ja/i;
		$url =~ s/ korean/ ko/i;
		$url =~ s/ portugese/ pt/i;
		$url =~ s/ russian/ ru/i;
		$url =~ s/ spanish/ es/i;
		my ($from_language, $to_language) = $url =~ /^[a-z]* (.*) (.*)$/;
		(undef, $current_url) = $current_url =~ /^(.*):\/\/(.*)/;
		$url = 'http://babelfish.altavista.com/babelfish/urltrurl?lp=' . $from_language . '_' . $to_language . '&url=' . $current_url;
		return $url;
	}

	# XYZZY
	if ($url =~ '^xyzzy$')
	{
		#$url = 'http://sundae.triumf.ca/pub2/cave/node001.html';
		srand();
		my $xyzzy = int(rand(6));
		   if ($xyzzy == 0) {$xyzzy = 1;}   # Colossal Cave Adventure
		elsif ($xyzzy == 1) {$xyzzy = 227;} # Zork Zero: The Revenge of Megaboz    (883 G.U.E.)
		elsif ($xyzzy == 2) {$xyzzy = 3;}   # Zork I: The Great Underground Empire (948 G.U.E.)
		elsif ($xyzzy == 3) {$xyzzy = 4;}   # Zork II: The Wizard of Frobozz       (948 G.U.E.)
		elsif ($xyzzy == 4) {$xyzzy = 5;}   # Zork III: The Dungeon Master         (948 G.U.E.)
		elsif ($xyzzy == 5) {$xyzzy = 6;}   # Zork: The Undiscovered Underground   (1066 G.U.E.)
		$url = 'http://ifiction.org/games/play.php?cat=2&game=' . $xyzzy . '&mode=html';
		return $url;
	}

	return $url;
}

################################################################################
### follow_url_hook ############################################################
sub follow_url_hook
{
	my $url = shift;

	# Bork! Bork! Bork!
	if ($url =~ 'google\.com')
	{
		if ($url =~ '^http://(|www\.|search\.)google\.com(|/search)(|/)$')
		{
			$url = 'http://google.com/webhp?hl=xx-bork';
		}
		elsif ($url =~ '^http://(|www\.)groups\.google\.com(|/groups)(|/)$'
			|| $url =~ '^http://(|www\.|search\.)google\.com/groups(|/)$')
		{
			$url = 'http://google.com/groups?hl=xx-bork';
		}
		return $url;
	}

	# nntp?  try Google groups
	if ($url =~ '^(nntp|news):')
	{
		my $beta;
		if (! -f $ENV{"HOME"} . '/.elinks/beta')
		{
			$beta = "groups"; # You lika de beta?  I no lika de beta.
		}
		else
		{
			$beta = "groups-beta"; # I like ugly sphincterfaces.
		}
		$url =~ s/\///g;
		my ($group) = $url =~ /[a-zA-Z]:(.*)/;
		$url = 'http://' . $beta . '.google.com/groups?hl=xx-bork&group=' . $group;
		return $url;
	}

	return $url;
}

################################################################################
### pre_format_html_hook #######################################################
sub pre_format_html_hook
{
	my $url = shift;
	my $html = shift;

	# /. sanitation
	if ($url =~ 'slashdot\.org')
	{
#		$html =~ s/^<!-- Advertisement code. -->.*<!-- end ad code -->$//sm;
#		$html =~ s/<iframe.*><\/iframe>//g;
#		$html =~ s/<B>Advertisement<\/B>//;
		return $html;
	}

	# yes, I heard you the first time
	if ($url =~ 'google\.com')
	{
		$html =~ s/Teep: In must broosers yuoo cun joost heet zee retoorn key insteed ooff cleecking oon zee seerch boottun\. Bork bork bork!//;
		$html =~ s/Tip:<\/font> Save time by hitting the return key instead of clicking on "search"/<\/font>/;
		return $html;
	}

	# SourceForge ad smasher
	if ($url =~ 'sourceforge\.net')
	{
		$html =~ s/<!-- AD POSITION \d+ -->.*?<!-- END AD POSITION \d+ -->//smg;
		$html =~ s/<b>&nbsp\;&nbsp\;&nbsp\;Site Sponsors<\/b>//g;
		return $html;
	}

	return $html;
}

################################################################################
### proxy_for_hook #############################################################
sub proxy_for_hook
{
	my $url = shift;

	# no proxy for local files
	if ($url =~ '^(file://|(http://|)(localhost|127\.0\.0\.1)(/|:|$))')
	{
		return "";
	}

	return;
}

################################################################################
### quit_hook ##################################################################
sub quit_hook
{
	# words of wisdom from ELinks the Sage
	my $cookiejar = 'elinks.fortune';
	my $ohwhynot = `ls /usr/share/doc/elinks*/$cookiejar 2>/dev/null`;
	open COOKIES, $ENV{"HOME"} . '/.elinks/' . $cookiejar
		or open COOKIES, '/etc/elinks/' . $cookiejar
		or open COOKIES, '/usr/share/elinks/' . $cookiejar
		or open COOKIES, $ohwhynot
		or die system('echo ""; fortune -sa 2>/dev/null');
	my (@line, $fortune);
	$line[0] = 0;
	while(<COOKIES>)
	{
		$line[$#line + 1] = tell if /^%$/;
	}
	srand();
	seek(COOKIES, $line[int rand($#line + 1)], 0);
	while(<COOKIES>)
	{
		last if /^%$/;
		$fortune .= $_;
	}
	close COOKIES;
	print("\n", $fortune);
}
