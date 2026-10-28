import importlib.util
import pathlib
import unittest

spec = importlib.util.spec_from_file_location('docs', pathlib.Path(__file__).resolve().parents[1] / 'gen_api_docs.py')
docs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(docs)


class DeclarationsTest(unittest.TestCase):
    def test_multiline_function_with_attribute(self):
        entries = list(docs.declarations('/** Formats text. */\nCBAT_API int log(\n const char *fmt, ... ) CBAT_PRINTF_FORMAT(1, 2);'))
        self.assertEqual(entries, [('CBAT_API int log( const char *fmt, ... ) CBAT_PRINTF_FORMAT(1, 2);', 'Formats text.')])

    def test_struct_fields_and_comments_do_not_end_declaration(self):
        text = '/** A value. */\ntypedef struct X { int a; /* ; } */ char *b; } X;\nCBAT_API void f(void);'
        entries = list(docs.declarations(text))
        self.assertEqual(len(entries), 2)
        self.assertTrue(entries[0][0].endswith('} X;'))
        self.assertEqual(entries[0][1], 'A value.')
        self.assertIsNone(entries[1][1])

    def test_comment_is_not_reused_for_next_function(self):
        entries = list(docs.declarations('/** First only. */ CBAT_API int a(void);\nCBAT_API int b(void);'))
        self.assertEqual([doc for _, doc in entries], ['First only.', None])

    def test_commented_out_api_and_string_are_ignored(self):
        text = '/* CBAT_API void hidden(void); */\n#define LABEL "CBAT_API"\nCBAT_API int visible(void);'
        self.assertEqual(list(docs.declarations(text)), [('CBAT_API int visible(void);', None)])

    def test_multiline_documentation(self):
        self.assertEqual(docs.clean_comment('/**\n * One.\n *\n * Two.\n */'), 'One.\n\nTwo.')

    def test_unterminated_declaration_is_an_error(self):
        with self.assertRaises(ValueError):
            list(docs.declarations('CBAT_API int broken(void)'))


if __name__ == '__main__':
    unittest.main()
