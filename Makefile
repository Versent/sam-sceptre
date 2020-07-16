noop:=
space:=$(noop) $(noop)


# default target
.PHONY: help
help:  ## Show this help
	@fgrep -h "##" $(MAKEFILE_LIST) | fgrep -v fgrep  | sed -e 's/:[^#]*##/ -- /; s/^##/\n##/'


.PHONY: pytest
pytest:  ## Run pytest unit tests 
	PYTHONPATH=$(subst $(space),:,$(wildcard ./templates/sam/*/code/)) pytest ./templates/sam/*/tests/

#
## sceptre targets
#
.PHONY: sceptre-artifact-bucket
sceptre-artifact-bucket:  ## create artifacts bucket for SAM packaging
	sceptre launch --yes "s3-artifact-bucket.yaml"


.PHONY: sceptre-lambda-echo
sceptre-lambda-echo: sam-package  ## launch lambda-echo stack
	sceptre launch --yes "lambda-echo.yaml"


#
## SAM targets
#

PACKAGE_BUCKET = $(shell aws cloudformation describe-stacks --stack-name my-project-s3-artifact-bucket --query 'Stacks[0].Outputs[?OutputKey==`ArtifactBucketName`].OutputValue' --output text )
SAM-SUBDIRS = $(patsubst %Makefile,%$(noop),$(wildcard ./templates/sam/*/Makefile))

.PHONY: sam-package
sam-package: sceptre-artifact-bucket $(SAM-SUBDIRS)  ## package all the SAM modules
	# no-op

.PHONY: $(SAM-SUBDIRS)
$(SAM-SUBDIRS):  ## `make package` for each SAM module; called by sam-package
	cd $@ ; \
	PACKAGE_BUCKET=$(PACKAGE_BUCKET) $(MAKE) package

.PHONY: clean
clean:  ## remove intermediate sam-package build files
	find ./templates/sam/ \
		-name '__generated*' \
		-o \
		-name '__pycache__' \
		-o \
		-name '.aws-sam' \
		-o \
		-name '.package_bucket.txt' \
	| xargs rm -rf
