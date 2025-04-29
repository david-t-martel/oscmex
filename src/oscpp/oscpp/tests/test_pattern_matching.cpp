#include <gtest/gtest.h>

#include "ServerImpl.h"
#include "osc/Address.h"

using namespace osc;

// Test class to expose the matchPattern function for testing
class PatternMatchingTest {
   public:
    // Create a proxy method to access the pattern matching function
    static bool matchPattern(const char* pattern, const char* path) {
        return ServerImpl::matchPattern(pattern, path);
    }
};

TEST(PatternMatching, ExactMatching) {
    // Basic exact matches
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/test", "/test"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/test/path", "/test/path"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/", "/"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("", ""));

    // Non-matches
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/test", "/test/extra"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/test/path", "/test"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/test", "/Test"));  // Case-sensitive
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/test/", "/test"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/test", "/test/"));
}

TEST(PatternMatching, WildcardQuestionMark) {
    // ? wildcard - matches any single character
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/te?t", "/test"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/te?t", "/text"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/?est", "/test"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/tes?", "/test"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/???", "/abc"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/test/pa?h", "/test/path"));

    // ? should not match '/'
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/te?t", "/te/t"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/?est/path", "/test"));

    // ? requires a character
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/te?t", "/tet"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/tes?", "/tes"));

    // Multiple ? wildcards
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/t??t", "/test"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/?e?t", "/test"));
}

TEST(PatternMatching, WildcardAsterisk) {
    // * wildcard - matches any sequence of zero or more characters except '/'
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/test*", "/test"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/test*", "/testABC"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/*test", "/ABCtest"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/te*st", "/test"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/te*st", "/teABCst"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/*", "/anything"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/test/pa*th", "/test/path"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/test/pa*th", "/test/paLongerth"));

    // * should not match '/'
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/te*st", "/te/st"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/*test", "/something/test"));

    // Multiple * wildcards
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/te*s*t", "/test"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/te*s*t", "/teABCst"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/te*s*t", "/teABCsDEFt"));
}

TEST(PatternMatching, WildcardCharacterClass) {
    // [abc] - matches one character which can be 'a', 'b', or 'c'
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/t[eao]st", "/test"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/t[eao]st", "/tast"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/t[eao]st", "/tost"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/t[eao]st", "/txst"));

    // [a-z] - matches one character in the given range
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/t[a-z]st", "/test"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/t[a-z]st", "/tast"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/t[a-z]st", "/tzst"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/t[a-z]st", "/tAst"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/t[a-z]st", "/t9st"));

    // [!abc] or [^abc] - matches one character which is not 'a', 'b', or 'c'
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/t[!eao]st", "/txst"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/t[^eao]st", "/txst"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/t[!eao]st", "/test"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/t[^eao]st", "/tast"));

    // [!a-z] or [^a-z] - matches one character not in the given range
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/t[!a-z]st", "/t9st"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/t[^a-z]st", "/tAst"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/t[!a-z]st", "/test"));

    // Character class should not match '/'
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/t[a/o]st", "/t/st"));

    // Multiple character classes
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/[abc][123]", "/a1"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/[abc][123]", "/c3"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/[abc][123]", "/d1"));

    // Empty character class should match nothing
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/t[]st", "/test"));

    // Character class with special characters
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/t[e*?]st", "/t*st"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/t[e*?]st", "/t?st"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/t[e*?]st", "/test"));
}

TEST(PatternMatching, WildcardAlternatives) {
    // {alt1,alt2,alt3} - matches if the path matches any of the comma-separated alternatives
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/{foo,bar,baz}", "/foo"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/{foo,bar,baz}", "/bar"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/{foo,bar,baz}", "/baz"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/{foo,bar,baz}", "/qux"));

    // Nested alternatives should work
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/{foo,ba{r,z}}", "/foo"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/{foo,ba{r,z}}", "/bar"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/{foo,ba{r,z}}", "/baz"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/{foo,ba{r,z}}", "/bat"));

    // Alternatives with wildcards
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/{te?t,rest}", "/test"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/{te?t,rest}", "/rest"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/{te*,re*}", "/testing"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/{te*,re*}", "/results"));

    // Alternatives in the middle of a pattern
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/test/{foo,bar}/end", "/test/foo/end"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/test/{foo,bar}/end", "/test/bar/end"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/test/{foo,bar}/end", "/test/baz/end"));

    // Empty alternative should match empty string
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/test/{a,b,}", "/test/"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/test/{a,b,}", "/test/a"));
}

TEST(PatternMatching, ComplexPatterns) {
    // Combining different wildcards
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/*/[abc]/?", "/test/a/x"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/*/[abc]/?", "/foo/c/y"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/*/[abc]/?", "/test/d/x"));

    // Complex patterns with nested alternatives
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/{foo*,b[aeiou]r,*baz}", "/foobar"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/{foo*,b[aeiou]r,*baz}", "/bar"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/{foo*,b[aeiou]r,*baz}", "/testbaz"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/{foo*,b[aeiou]r,*baz}", "/bat"));

    // More complex paths
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/[a-z]*/[0-9]/{on,off}", "/abc/123/on"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/[a-z]*/[0-9]/{on,off}", "/xyz/7/off"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/[a-z]*/[0-9]/{on,off}", "/abc/123/standby"));

    // Real-world OSC address patterns
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/synth/[0-9]/volume", "/synth/1/volume"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/effect/*/param", "/effect/delay/param"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/mixer/{gain,pan,mute}", "/mixer/gain"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/track/[1-8]/send/[1-4]", "/track/3/send/2"));
}

TEST(PatternMatching, EdgeCases) {
    // Empty pattern and path
    EXPECT_TRUE(PatternMatchingTest::matchPattern("", ""));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("", "/test"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/test", ""));

    // Root path
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/", "/"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/", "/test"));

    // Pattern with only wildcards
    EXPECT_TRUE(PatternMatchingTest::matchPattern("*", "anything"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("?", "a"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("[a-z]", "q"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("{a,b,c}", "b"));

    // Unclosed character class or alternatives
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/test/[abc", "/test/a"));
    EXPECT_FALSE(PatternMatchingTest::matchPattern("/test/{a,b", "/test/a"));

    // Special cases
    EXPECT_TRUE(PatternMatchingTest::matchPattern("//", "//"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("///", "///"));
    EXPECT_TRUE(PatternMatchingTest::matchPattern("/*//", "/test//"));
}
