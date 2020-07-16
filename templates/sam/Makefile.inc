ifndef INPUT_TEMPLATE_FILE
$(error "INPUT_TEMPLATE_FILE must be defined (as variable in caller Makefile)")
endif

ifndef PACKAGE_BUCKET
$(error "PACKAGE_BUCKET must be defined (as environment variable)")
endif

# package manifest file used by `sam build`
# file must exist, but may be empty
# default to python manifest file
# may be overridden by caller Makefile (e.g. for nodejs: ./code/package.json)
MANIFEST_FILE ?= ./code/requirements.txt

# The output CloudFormation template that includes references to the packaged code in S3
OUTPUT_TEMPLATE_FILE := __generated__.yaml

SOURCE_FILES := $(shell find . -type f -path './code/*' )


# Build dependency on the value of PACKAGE_BUCKET environment variable
# https://stackoverflow.com/a/59787919/358224
SHELL := /bin/bash
var_file := .package_bucket.txt
var_name := PACKAGE_BUCKET
is_var_updated = [[ ! -e $(var_file) ]] || [[ "$$(< $(var_file))" != "$($(var_name))" ]]
update_var_file = echo "$($(var_name))" > $(var_file)
$(shell $(is_var_updated) && $(update_var_file))


.PHONY: help
help:  ## Show this help.
	@fgrep -h "##" $(MAKEFILE_LIST) | fgrep -v fgrep | sed -e 's/\\$$//; s/:[^#]*##/:\t/'

# SAM_CLI_TELEMETRY=0 => Turn off telemetry warnings https://docs.aws.amazon.com/serverless-application-model/latest/developerguide/serverless-sam-telemetry.html

.aws-sam/build/template.yaml: $(INPUT_TEMPLATE_FILE) $(SOURCE_FILES)  ## sam-build target and dependencies
	SAM_CLI_TELEMETRY=0 \
	sam build \
		--template-file "$(INPUT_TEMPLATE_FILE)" \
		--manifest "$(MANIFEST_FILE)" \
		--debug

$(OUTPUT_TEMPLATE_FILE): $(var_file) .aws-sam/build/template.yaml  ## create and upload the SAM application-bundle 
	SAM_CLI_TELEMETRY=0 \
	sam package \
		--s3-bucket "$(PACKAGE_BUCKET)" \
		--output-template-file "$(OUTPUT_TEMPLATE_FILE)" \
		--debug

.PHONY: package
package:: $(OUTPUT_TEMPLATE_FILE)  ## standard target to be invoked: package the application
	@echo "*** finished packaging $(INPUT_TEMPLATE_FILE)"
