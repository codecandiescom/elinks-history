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
		my (undef, $target_url) = $current_url =~ /^(.*):\/\/(.*)/;
		$url = 'http://bugmenot.com/view.php?url=' . $target_url;
		return $url;
	}

	# random URL generator
	if ($url =~ '^bored$' || $url =~ '^random$')
	{
		my $word; # you can say *that* again...
		srand();
		open FILE, '</usr/share/dict/words'
			or open FILE, '</usr/dict/words'
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

	# MirrorDot dumb prefix
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

	# torrent search
	if ($url =~ '^(bittorrent|torrent|bt)( .*)$')
	{
		my ($torrent) = $url =~ /^[a-z]* (.*)/;
		$url = 'http://google.com/search?q=filetype:torrent ' . $torrent . '&hl=xx-bork';
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
#		$html =~ s/^<!-- Advertisement code. -->.*<!-- end ad code -->$/<br>/sm;
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
	system('fortune -sa 2>/dev/null');
}
