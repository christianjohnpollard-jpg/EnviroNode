EnviroNode_Handler.py - This is the python script that works in Lambda to parse the json payload, validate the contents, and pass them through to influx db. To be able to run this you will need to upload a python influxdb layer. I have included this in the repo.

influxdb-layer.zip - This is the layer that you will need to upload to AWS when you are setting up the lambda python 3.14 environment.

lambda_role_policy.json - This is the policy in json form that is attached to the lambda handler. This will help you identify whether or not lambda is parsing the payloads correctly as it will be recorded in cloudwatch logs.


