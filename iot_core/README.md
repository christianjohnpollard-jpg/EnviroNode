enviro_node_ingest - This is the iot rule SQL script that you use to select everything from the mqtt topic that the esp32 is publishing to.

You will need to make the following actions for the enviro_node_ingest iot core rule:

1. S3 bucket - Store a message in an Amazon S3 bucket
2. lambda - Send a message to a Lambda function
3. cloudwatch logs - Send message data to CloudWatch logs
