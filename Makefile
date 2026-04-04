.PHONY: build release flash setup clean monitor test help

build:        ## Compile firmware, report sizes
	./build.sh

release:      ## Compile + create versioned DFU zip in releases/
	./build.sh --release

flash:        ## Compile + flash via USB serial
	./build.sh --flash

setup:        ## Install PlatformIO + adafruit-nrfutil
	./build.sh --setup

clean:        ## Remove build artifacts
	rm -rf .pio/

monitor:      ## Open serial monitor at 115200 baud
	pio device monitor

test:         ## Run host-side PlatformIO unit tests (native)
	pio test -e native

help:         ## Show available targets
	@grep -E '^[a-z]+:.*##' $(MAKEFILE_LIST) | sed 's/:.*## /\t/' | column -t -s '	'
