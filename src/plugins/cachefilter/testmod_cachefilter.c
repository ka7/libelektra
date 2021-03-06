/**
 * @file
 *
 * @brief Tests for cachefilter plugin
 *
 * @copyright BSD License (see doc/LICENSE.md or http://www.libelektra.org)
 *
 */

#include <stdlib.h>
#include <string.h>

#include <tests_internal.h>

#include <kdbconfig.h>
#include <kdbinternal.h>

#include <tests_plugin.h>

static KeySet * createTestKeysToCache ()
{
	return ksNew (3, keyNew ("user/tests/cachefilter/will/be/cached/key1", KEY_VALUE, "cached1", KEY_END),
		      keyNew ("user/tests/cachefilter/will/be/cached/key2", KEY_VALUE, "cached2", KEY_END),
		      keyNew ("user/tests/cachefilter/will/be/cached", KEY_VALUE, "cached", KEY_END), KS_END);
}

static KeySet * createTestKeysToCache2 ()
{
	return ksNew (3, keyNew ("user/tests/cachefilter/will/be/cached/key3", KEY_VALUE, "cached1", KEY_END),
		      keyNew ("user/tests/cachefilter/will/be/cached/key4", KEY_VALUE, "cached2", KEY_END),
		      keyNew ("user/tests/cachefilter/will/be/cached/key5", KEY_VALUE, "cached", KEY_END), KS_END);
}

static KeySet * createTestKeysToCache3 ()
{
	return ksNew (3, keyNew ("user/tests/cachefilter/will/be/cached/key6", KEY_VALUE, "cached1", KEY_END),
		      keyNew ("user/tests/cachefilter/will/be/cached/key7", KEY_VALUE, "cached2", KEY_END),
		      keyNew ("user/tests/cachefilter/will/be/cached/key8", KEY_VALUE, "cached", KEY_END), KS_END);
}

static KeySet * createTestKeysToNotCache ()
{
	return ksNew (3, keyNew ("user/tests/cachefilter/will/not/be/cached/key1", KEY_VALUE, "not-cached1", KEY_END),
		      keyNew ("user/tests/cachefilter/will/not/be/cached/key2", KEY_VALUE, "not-cached2", KEY_END),
		      keyNew ("user/tests/cachefilter/will/not/be/cached", KEY_VALUE, "not-cached", KEY_END), KS_END);
}

static void test_successfulCache ()
{
	Key * parentKey = keyNew ("user/tests/cachefilter/will/not/be/cached", KEY_END);
	KeySet * conf = ksNew (0, KS_END);
	PLUGIN_OPEN ("cachefilter");

	KeySet * testKeysCache = createTestKeysToCache ();
	KeySet * testKeysNoCache = createTestKeysToNotCache ();

	KeySet * ks = ksNew (0, KS_END);
	ksAppend (ks, testKeysCache);
	ksAppend (ks, testKeysNoCache);

	succeed_if (plugin->kdbGet (plugin, ks, parentKey) >= 1, "call to kdbGet was not successful");
	succeed_if (output_error (parentKey), "error in kdbGet");
	succeed_if (output_warnings (parentKey), "warnings in kdbGet");
	succeed_if (ksGetSize (ks) == 3, "wrong number of keys in result, expected 3");

	KeySet * expected = ksNew (0, KS_END);
	ksAppend (expected, testKeysNoCache);
	compare_keyset (ks, expected);
	ksDel (expected);

	// kdbSet() result >= 0 because nothing had to be done and should be successful
	succeed_if (plugin->kdbSet (plugin, ks, parentKey) >= 0, "call to kdbSet was not successful");
	succeed_if (output_error (parentKey), "error in kdbGet");
	succeed_if (output_warnings (parentKey), "warnings in kdbGet");
	succeed_if (ksGetSize (ks) == 6, "wrong number of keys in result, expected 6");

	expected = ksNew (0, KS_END);
	ksAppend (expected, testKeysNoCache);
	ksAppend (expected, testKeysCache);
	compare_keyset (ks, expected);
	ksDel (expected);

	keyDel (parentKey);
	ksDel (ks);

	ksDel (testKeysCache);
	ksDel (testKeysNoCache);

	PLUGIN_CLOSE ();
}

static void test_successfulCacheLong ()
{
	Key * parentKey = keyNew ("user/tests/cachefilter/will/not/be/cached", KEY_END);
	KeySet * conf = ksNew (0, KS_END);
	PLUGIN_OPEN ("cachefilter");

	KeySet * testKeysCache = createTestKeysToCache ();
	KeySet * testKeysCache2 = createTestKeysToCache2 ();
	KeySet * testKeysCache3 = createTestKeysToCache3 ();
	KeySet * testKeysNoCache = createTestKeysToNotCache ();

	KeySet * ks = ksNew (0, KS_END);
	ksAppend (ks, testKeysCache);
	ksAppend (ks, testKeysNoCache);

	succeed_if (plugin->kdbGet (plugin, ks, parentKey) >= 1, "call to kdbGet was not successful");
	succeed_if (output_error (parentKey), "error in kdbGet");
	succeed_if (output_warnings (parentKey), "warnings in kdbGet");
	succeed_if (ksGetSize (ks) == 3, "wrong number of keys in result, expected 3");

	KeySet * expected = ksNew (0, KS_END);
	ksAppend (expected, testKeysNoCache);
	compare_keyset (ks, expected);
	ksDel (expected);

	// request some more keys in order to cache more
	ksAppend (ks, testKeysCache2);

	succeed_if (plugin->kdbGet (plugin, ks, parentKey) >= 1, "call to kdbGet was not successful 2");
	succeed_if (output_error (parentKey), "error in kdbGet");
	succeed_if (output_warnings (parentKey), "warnings in kdbGet");
	succeed_if (ksGetSize (ks) == 3, "wrong number of keys in result, expected 3");

	expected = ksNew (0, KS_END);
	ksAppend (expected, testKeysNoCache);
	compare_keyset (ks, expected);
	ksDel (expected);

	// and even more ...
	ksAppend (ks, testKeysCache3);

	succeed_if (plugin->kdbGet (plugin, ks, parentKey) >= 1, "call to kdbGet was not successful 3");
	succeed_if (output_error (parentKey), "error in kdbGet");
	succeed_if (output_warnings (parentKey), "warnings in kdbGet");
	succeed_if (ksGetSize (ks) == 3, "wrong number of keys in result, expected 3");

	expected = ksNew (0, KS_END);
	ksAppend (expected, testKeysNoCache);
	compare_keyset (ks, expected);
	ksDel (expected);

	// kdbSet() result >= 0 because nothing had to be done and should be successful
	succeed_if (plugin->kdbSet (plugin, ks, parentKey) >= 0, "call to kdbSet was not successful");
	succeed_if (output_error (parentKey), "error in kdbGet");
	succeed_if (output_warnings (parentKey), "warnings in kdbGet");
	succeed_if (ksGetSize (ks) == 12, "wrong number of keys in result, expected 12");

	expected = ksNew (0, KS_END);
	ksAppend (expected, testKeysNoCache);
	ksAppend (expected, testKeysCache);
	ksAppend (expected, testKeysCache2);
	ksAppend (expected, testKeysCache3);
	compare_keyset (ks, expected);
	ksDel (expected);

	keyDel (parentKey);
	ksDel (ks);

	ksDel (testKeysCache);
	ksDel (testKeysCache2);
	ksDel (testKeysCache3);
	ksDel (testKeysNoCache);

	PLUGIN_CLOSE ();
}

static void test_successfulGetSetGetSet ()
{
	Key * parentKey = keyNew ("user/tests/cachefilter/will/not/be/cached", KEY_END);
	KeySet * conf = ksNew (0, KS_END);
	PLUGIN_OPEN ("cachefilter");

	KeySet * testKeysCache = createTestKeysToCache ();
	KeySet * testKeysNoCache = createTestKeysToNotCache ();

	KeySet * ks = ksNew (0, KS_END);
	ksAppend (ks, testKeysCache);
	ksAppend (ks, testKeysNoCache);

	// kdbGet() first
	succeed_if (plugin->kdbGet (plugin, ks, parentKey) >= 1, "call to kdbGet was not successful");
	succeed_if (output_error (parentKey), "error in kdbGet");
	succeed_if (output_warnings (parentKey), "warnings in kdbGet");
	succeed_if (ksGetSize (ks) == 3, "wrong number of keys in result, expected 3");

	KeySet * expected = ksNew (0, KS_END);
	ksAppend (expected, testKeysNoCache);
	compare_keyset (ks, expected);
	ksDel (expected);

	// kdbSet() result >= 0 because nothing had to be done and should be successful
	succeed_if (plugin->kdbSet (plugin, ks, parentKey) >= 0, "call to kdbSet was not successful");
	succeed_if (output_error (parentKey), "error in kdbGet");
	succeed_if (output_warnings (parentKey), "warnings in kdbGet");
	succeed_if (ksGetSize (ks) == 6, "wrong number of keys in result, expected 6");

	expected = ksNew (0, KS_END);
	ksAppend (expected, testKeysNoCache);
	ksAppend (expected, testKeysCache);
	compare_keyset (ks, expected);
	ksDel (expected);

	// another kdbGet() call
	succeed_if (plugin->kdbGet (plugin, ks, parentKey) >= 1, "call to kdbGet was not successful");
	succeed_if (output_error (parentKey), "error in kdbGet");
	succeed_if (output_warnings (parentKey), "warnings in kdbGet");
	succeed_if (ksGetSize (ks) == 3, "wrong number of keys in result, expected 3");

	expected = ksNew (0, KS_END);
	ksAppend (expected, testKeysNoCache);
	compare_keyset (ks, expected);
	ksDel (expected);

	// another kdbSet() call
	succeed_if (plugin->kdbSet (plugin, ks, parentKey) >= 0, "call to kdbSet was not successful");
	succeed_if (output_error (parentKey), "error in kdbGet");
	succeed_if (output_warnings (parentKey), "warnings in kdbGet");
	succeed_if (ksGetSize (ks) == 6, "wrong number of keys in result, expected 6");

	expected = ksNew (0, KS_END);
	ksAppend (expected, testKeysNoCache);
	ksAppend (expected, testKeysCache);
	compare_keyset (ks, expected);
	ksDel (expected);

	keyDel (parentKey);
	ksDel (ks);

	ksDel (testKeysCache);
	ksDel (testKeysNoCache);

	PLUGIN_CLOSE ();
}

int main (int argc, char ** argv)
{
	printf ("CACHEFILTER     TESTS\n");
	printf ("==================\n\n");

	init (argc, argv);

	test_successfulCache ();
	test_successfulCacheLong ();
	test_successfulGetSetGetSet ();

	printf ("\ntestmod_cachefilter RESULTS: %d test(s) done. %d error(s).\n", nbTest, nbError);

	return nbError;
}
