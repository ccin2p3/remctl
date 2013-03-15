# Configuration for Perl test cases.
#
# In order to reuse the same Perl test cases in multiple packages, I use a
# configuration file to store some package-specific data.  This module loads
# that configuration and provides the namespace for the configuration
# settings.
#
# The canonical version of this file is maintained in the rra-c-util package,
# which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.

package Test::RRA::Config;

use 5.006;
use strict;
use warnings;

# For Perl 5.006 compatibility.
## no critic (ClassHierarchies::ProhibitExplicitISA)

use Exporter;
use Test::More;

# Declare variables that should be set in BEGIN for robustness.
our (@EXPORT_OK, @ISA, $VERSION);

# Set $VERSION and everything export-related in a BEGIN block for robustness
# against circular module loading (not that we load any modules, but
# consistency is good).
BEGIN {
    @ISA       = qw(Exporter);
    @EXPORT_OK = qw(
      $COVERAGE_LEVEL @COVERAGE_SKIP_TESTS @CRITIC_IGNORE $LIBRARY_PATH
      $MINIMUM_VERSION %MINIMUM_VERSION @POD_COVERAGE_EXCLUDE
    );

    # This version should match the corresponding rra-c-util release, but with
    # two digits for the minor version, including a leading zero if necessary,
    # so that it will sort properly.
    $VERSION = '4.08';
}

# If BUILD or SOURCE are set in the environment, look for data/perl.conf under
# those paths for a C Automake package.  Otherwise, look in t/data/perl.conf
# for a standalone Perl module.  Don't use Test::RRA::Automake since it may
# not exist.
our $PATH;
for my $base ($ENV{BUILD}, $ENV{SOURCE}, 't') {
    next if !defined($base);
    my $path = "$base/data/perl.conf";
    if (-r $path) {
        $PATH = $path;
        last;
    }
}
if (!defined($PATH)) {
    BAIL_OUT('cannot find data/perl.conf');
}

# Pre-declare all of our variables and set any defaults.
our $COVERAGE_LEVEL = 100;
our @COVERAGE_SKIP_TESTS;
our @CRITIC_IGNORE;
our $LIBRARY_PATH;
our $MINIMUM_VERSION = '5.008';
our %MINIMUM_VERSION;
our @POD_COVERAGE_EXCLUDE;

# Load the configuration.
if (!do($PATH)) {
    my $error = $@ || $! || 'loading file did not return true';
    BAIL_OUT("cannot load data/perl.conf: $error");
}

1;
__END__

=for stopwords
Allbery rra-c-util Automake perlcritic .libs namespace sublicense
MERCHANTABILITY NONINFRINGEMENT

=head1 NAME

Test::RRA::Config - Perl test configuration

=head1 SYNOPSIS

    use Test::RRA::Config qw($MINIMUM_VERSION);
    print "Required Perl version is $MINIMUM_VERSION\n";

=head1 DESCRIPTION

Test::RRA::Config encapsulates per-package configuration for generic Perl
test programs that are shared between multiple packages using the
rra-c-util infrastructure.  It handles locating and loading the test
configuration file for both C Automake packages and stand-alone Perl
modules.

Test::RRA::Config looks for a file named F<data/perl.conf> relative to the
root of the test directory.  That root is taken from the environment
variables BUILD or SOURCE (in that order) if set, which will be the case
for C Automake packages using C TAP Harness.  If neither is set, it
expects the root of the test directory to be a directory named F<t>
relative to the current directory, which will be the case for stand-alone
Perl modules.

The following variables are supported:

=over 4

=item $COVERAGE_LEVEL

The coverage level achieved by the test suite for Perl test coverage
testing using Test::Strict, as a percentage.  The test will fail if test
coverage less than this percentage is achieved.  If not given, defaults
to 100.

=item @COVERAGE_SKIP_TESTS

Directories under F<t> whose tests should be skipped when doing coverage
testing.  This can be tests that won't contribute to coverage or tests
that don't run properly under Devel::Cover for some reason (such as ones
that use taint checking).  F<docs> and F<style> will always be skipped
regardless of this setting.

=item @CRITIC_IGNORE

Additional directories to ignore when doing recursive perlcritic testing.
The contents of this directory must be either top-level directory names or
directory names starting with F<tests/>.

=item $LIBRARY_PATH

Add this directory (or a .libs subdirectory) relative to the top of the
source tree to LD_LIBRARY_PATH when checking the syntax of Perl modules.
This may be required to pick up libraries that are used by in-tree Perl
modules so that Perl scripts can pass a syntax check.

=item $MINIMUM_VERSION

Default minimum version requirement for included Perl scripts.  If not
given, defaults to 5.008.

=item %MINIMUM_VERSION

Minimum version exceptions for specific directories.  The keys should be
minimum versions of Perl to enforce.  The value for each key should be a
reference to an array of either top-level directory names or directory
names starting with F<tests/>.  All files in those directories will have
that minimum Perl version constraint imposed instead of $MINIMUM_VERSION.

=item @POD_COVERAGE_EXCLUDE

Regexes that match method names that should be excluded from POD coverage
testing.  Normally, all methods have to be documented in the POD for a
Perl module, but methods matching any of these regexes will be considered
private and won't require documentation.

=back

No variables are exported by default, but the variables can be imported
into the local namespace to avoid long variable names.

=head1 AUTHOR

Russ Allbery <rra@stanford.edu>

=head1 COPYRIGHT AND LICENSE

Copyright 2013 The Board of Trustees of the Leland Stanford Junior
University.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

=head1 SEE ALSO

Test::RRA(3), Test::RRA::Automake(3)

This module is maintained in the rra-c-util package.  The current version
is available from L<http://www.eyrie.org/~eagle/software/rra-c-util/>.

The C TAP Harness test driver and libraries for TAP-based C testing are
available from L<http://www.eyrie.org/~eagle/software/c-tap-harness/>.

=cut