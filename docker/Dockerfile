FROM alpine:edge
MAINTAINER Pranav Shikarpur <contact@snpranav.com>

RUN apk --update --no-cache add htop && \
	rm -rf /var/lib/apt/lists/*

ENTRYPOINT ["htop"]
