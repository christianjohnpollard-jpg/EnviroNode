EnviroNode_Handler.py - This is the python script that works in Lambda to parse the json payload, validate the contents, and pass them through to influx db. To be able to run this you will need to upload a python influxdb layer. I have included this in the repo.

influxdb-layer.zip - This is the layer that you will need to upload to AWS when you are setting up the lambda python 3.14 environment.

lambda_role_policy.json - This is the policy in json form that is attached to the lambda handler. This will help you identify whether or not lambda is parsing the payloads correctly as it will be recorded in cloudwatch logs.

test_payload.json - This is a test payload that you can use to test the handler and look at the results in the cloudwatch logstream. Please note you will have to make sure the timestamp in the test payload is within 1 month of the day you perform the test as there is date validation in the handler script.
