
PROJECT_ID=farms-arduino-270802
gcloud config set project $PROJECT_ID
gcloud functions deploy insertFirmwaresOnBigquery --runtime nodejs14 --trigger-resource esp-ota-firmwares --trigger-event google.storage.object.finalize
gcloud functions deploy getDownloadUrl --runtime nodejs14 --trigger-http
