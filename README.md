# Cloud Run bandwidth debugging

This is a simple repro case that downloads 500 blobs using a thread pool.

## GCE

On GCE, prepare an e2 machine with 4 vCPU and 8 GiB of RAM) in
australia-southeast1, using Ubuntu 20.04 and full access to Cloud APIs. Connect
through ssh and run:

```bash
sudo apt update && sudo apt install -y docker.io git screen time
sudo usermod -aG docker $USER
```

Log out and back in again.

```bash
git clone --recursive https://github.com/lgruen/cloud-run-bandwidth.git
cd cloud-run-bandwidth
docker build --tag cloud-run-bandwidth .
```

Using `screen` or similar, run in one terminal:

```bash
docker run --init -it -p 8080:8080 cloud-run-bandwidth
```

In another terminal, run:

```bash
/usr/bin/time -v curl localhost:8080
```

The reported time should be about 5s.

## Cloud Run

Clone and deploy:

```bash
git clone --recursive https://github.com/lgruen/cloud-run-bandwidth.git
cd cloud-run-bandwidth
gcloud beta run deploy --region=australia-southeast1 --no-allow-unauthenticated --concurrency=1 --max-instances=100 --cpu=4 --memory=8Gi cloud-run-bandwidth --source .
```

To invoke, run:

```bash
CLOUD_RUN_URL=$(gcloud run services describe cloud-run-bandwidth --platform managed --region australia-southeast1 --format 'value(status.url)')
IDENTITY_TOKEN=$(gcloud auth print-identity-token)
time curl -H "Authorization: Bearer $IDENTITY_TOKEN" $CLOUD_RUN_URL
```

In contrast, this will take longer than a minute, typically about 100s.

Detailed timings, including for each GCS download, are visible in the
[Cloud Run logs](https://console.cloud.google.com/run/detail/australia-southeast1/cloud-run-bandwidth/logs).
