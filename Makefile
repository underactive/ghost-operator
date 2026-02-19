.PHONY: build release flash setup clean help

build:        ## Compile firmware, report sizes
	./build.sh

release:      ## Compile + create versioned DFU zip in releases/
	./build.sh --release

flash:        ## Compile + flash via USB serial
	./build.sh --flash

setup:        ## Install arduino-cli, board package, libraries
	./build.sh --setup

clean:        ## Remove build artifacts
	rm -rf build/

help:          ## Show available targets
	@grep -E '^[a-z]+:.*##' $(MAKEFILE_LIST) | sed 's/:.*## /\t/' | column -t -s '	'
