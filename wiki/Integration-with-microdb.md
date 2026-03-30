# Integration with microdb

The usual pattern is:

1. compress structured data before storage
2. store the compressed byte slice in `microdb`
3. load raw bytes back from `microdb`
4. decode into the target typed buffer

Delta encoding is especially useful for slowly changing sensor arrays.
