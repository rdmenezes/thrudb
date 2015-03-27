The S3 backend provides an extremely scalable and fast-ish (if you're in EC2) persistent storage backend.
Features

  * Massively Scalable
  * Simple setup, just provide your AWS credentials and bucket (tablename).

Future Direction & Development

  * s3 client replacement/rewrite

Use & Configuration

```
    AWS_ACCESS_KEY = <aws_access_key>
    AWS_SECRET_ACCESS_KEY = <secret_access_key>
    S3_BUCKET_PREFIX = thrudoc_prefix_
```