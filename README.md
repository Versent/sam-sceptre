# SAM and Sceptre

This repo gives a example skeleton for deploying AWS Lambda resources in CloudFormation templates or
[SAM](https://aws.amazon.com/serverless/sam/)
templates using CloudReach's [Sceptre](https://sceptre.cloudreach.com/).

The goal is to store the Lambda function source code in a a separate file (or files) from the CloudFormation template.
This has many advantages including:

- Allowing unit testing of the code (e.g. [pytest](https://docs.pytest.org/en/stable/))
- Allowing static linting of the code
- IDE support for syntax highlighting
- Permit library packaging

## Getting started

### Prerequisites

- make - standard on UNIX platforms. For Windows, consider docker
- [AWS SAM CLI](https://docs.aws.amazon.com/serverless-application-model/latest/developerguide/serverless-sam-cli-install.html)
- [sceptre CLI](https://sceptre.cloudreach.com/latest/docs/install.html)

### Hello World

Run `make sceptre-lambda-echo` from the top-level directory.

This will deploy a stack containing a simple Lambda function defined in `./templates/sam/lambda_echo/`.

Also deployed is an S3 Bucket for storing the packaged (ZIP file) Lambda function for deployment.

A naive `./config/config.yaml` is provided to select us-west-2 as the region and `my-project` as the project_code (/stack name prefix).

## Lambda resources

CloudFormation templates containing AWS::Serverless::Function or AWS::Lambda::Function resources are, by convention,
stored in sub-folders of `./templates/sam/`.
Each sub-folder is an application with a single CloudFormation Template.
Templates typically have a single function, but may include multiple.

While not strictly coupled to SAM CloudFormation template, the `sam` model provides gives a way to decouple
the source code from the CloudFormation template. Instead of inline source code or external S3 references,
the CloudFormation Template contains only references to local source code folders.

## Packaging

Both the AWS CLI and the SAM CLI support packaging Lambda functions.
Each CLI creates a ZIP file of the source and uploading it to S3.
The source template is then rewritten to replace local references with S3 references.

This repo uses the SAM CLI as it has more features, notably support for packaging manifest files for 3rd party libraries
(python's requirements.txt, nodejs' package.json).

## Lambda application structure

```text
./templates/sam/<app-name>/
├── code/  # application source code
│   ├── <app-name>.py  # implementation; may have a different name or be in package structure
│   └── requirements.txt  # pip dependencies; file is required, but may be empty.
├── Makefile  # makefile to build the SAM app; required
├── <app-name>.sam.yaml  # The CloudFormation SAM template
├── __generated__.yaml  # Generated CFN template file ; .gitignore-d at the parent level
├── sample-events/  # optional, but it often useful to keep sample-event json files for reference or `sam local invoke`
│   └── ....event.json
└── tests/  # optional: unit tests
    └── test_<app-name>.py  # unit test implementation; may have a different name or be in package structure
```

### templates/sam/< app-name >/< app-name >.sam.yaml

Each application will have a single CloudFormation template with one (or more) Lambda Functions and
whatever supporting Resources it requires.

A true SAM application will use the `Transform: AWS::Serverless-2016-10-31` declaration and can then use
`AWS::Serverless::*` resources, including `AWS::Serverless::Function`.

A example SAM template snippet, with local `CodeUri` property looks like:

```yaml
AWSTemplateFormatVersion: '2010-09-09'
Transform: AWS::Serverless-2016-10-31

Resources:
  MyApp:
    Type: AWS::Serverless::Function
    Properties:
      CodeUri: ./code/  # local folder reference
      Handler: lambda_function.lambda_handler
      Runtime: python3.7
```

This repository does not mandate the use of SAM templates. You can omit the Transform declaration use vanilla
CloudFormation `AWS::Lambda::Function` resources.

A vanilla CloudFormation template with local `Code` property looks like:

```yaml
AWSTemplateFormatVersion: '2010-09-09'
Transform: AWS::Serverless-2016-10-31

Resources:
  MyApp:
    Type: AWS::Lambda::Function
    Properties:
      Code: ./code/  # local folder reference
      Handler: lambda_function.lambda_handler
      Role: !GetAtt MyAppRole.Arn
      Runtime: python3.7
```

### templates/sam/< app-name >/Makefile

The Makefile in each app directory has the application configuration details.

A typical, and usually sufficient, Makefile looks like:

```Makefile
INPUT_TEMPLATE_FILE := <app-name>.sam.yaml

include ../Makefile.inc
```

A set of standard Makefile targets are `include`d from  `../Makefile.inc` file. See next section.

#### templates/sam/Makefile.inc

The `package` target will do a smart build of the application using the SAM CLI.
Makefile dependencies are used to avoid unnecessary rebuild steps.
There is an implicit remote dependency on the application bundle in S3 (see Troubleshooting, below).

Typically you can treat ../Makefile.inc as a black-box.

#### Packaging the SAM applications

This is covered by the `sam-package` target in the top-level `../../Makefile`.
This target traverses each `./templates/sam/<app-name>` directory and runs `make package`
providing the name of a writeable S3 `PACKAGE_BUCKET` as environment variable.

### __generated__.yaml

The output of the packaging process is a CloudFormation template with local source-code folder references
replaced with S3 URLs pointing to bundled artifacts in the PACKAGE_BUCKET.

This file is excluded from source control by `./templates/sam/.gitignore`.

## Sceptre

Sceptre can provision SAM applications using references to the `__generated__.yaml` templates

```yaml
---
template_path: sam/<app-name>/__generated__.yaml

```

## Unit tests

The top-level `Makefile` has a pytest target that will run the pytest unit testing framework over all
code in `/templates/sam/*/code/`.

## Advanced topics

### Non-python runtimes

By default, the sam packaging expects a (possibly empty) python `./code/requirements.txt` file defining the 3rd party library dependencies.

This needs to be substituted with the manifest file for the corresponding runtime. For example, for node.js add the following to the Makefile:

```Makefile
MANIFEST_FILE ?= ./code/package.json
```

And create the referenced `./code/package.json` file.

### Optional Makefile targets

You can optionally append additional Makefile targets, for example for local testing:

```Makefile
LAMBDA_RESOURCE_NAME := MyApp

sam-local-invoke: ## invoke the lambda function using 'sam local'
    sam local invoke \
        "$(LAMBDA_RESOURCE_NAME)" \
        --skip-pull-image \
        --event ./sample-events/test01.event.json --debug
```

## Multi-region support

The following Makefile supports deployment to us-east-1 in addition to the default region.

```Makefile
INPUT_TEMPLATE_FILE := <app-name>.sam.yaml

ifndef PACKAGE_BUCKET_US_EAST_1
$(error "PACKAGE_BUCKET_US_EAST_1 must be defined (as environment variable)")
endif

OUTPUT_TEMPLATE_FILE_US_EAST_1 := __generated__.us-east-1.yaml

include ../Makefile.inc

$(OUTPUT_TEMPLATE_FILE_US_EAST_1): $(var_file) .aws-sam/build/template.yaml  ## create and upload the SAM application-bundle
    SAM_CLI_TELEMETRY=0 \
    sam package \
        --s3-bucket "$(PACKAGE_BUCKET_US_EAST_1)" \
        --output-template-file "$(OUTPUT_TEMPLATE_FILE_US_EAST_1)" \
        --debug

# double-colon (::) allows multiple package targets defined in the included Makefile.inc and also this target.
# Makefile.inc::package will be invoked before this target.
package:: $(OUTPUT_TEMPLATE_FILE_US_EAST_1)  ## standard target to be invoked: package the application
    @echo "*** finished packaging ${INPUT_TEMPLATE_FILE} for us-east-1"
```

## Troubleshooting

### `FileNotFoundError: [Errno 2] No such file or directory: /project/templates/sam/.../__generated__.yaml` <!-- cspell:disable-line -->

This indicates that the corresponding application has not been run yet. Run:

```shell
make sam-package
```

### my-project-artifacts-us-west-2 AccessDenied errors

An error like:

```text
UPDATE_FAILED Your access has been denied by S3, please make sure your request credentials have permission to GetObject for my-project-artifacts-us-west-2/68dfa11078b03647bbb6f6c2bd740e8b. S3 Error Code: AccessDenied. S3 Error Message: Access Denied (Service: AWSLambdaInternal; Status Code: 403; Error Code: AccessDeniedException; Request ID: 6225d338-5170-4b2f-b7f7-8e96bef8388f)
```

Does not actually indicate a permission error. It likely means that you have a previously done a `make sam-package` and
that the (SAM build) artifacts pointed to by your `./templates/sam/__generated*.yaml` files no longer exist.

1. Run `make clean sam-package`
1. Retry your sceptre command
