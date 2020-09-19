FROM alpine:3.12
MAINTAINER Pranav Shikarpur <contact@snpranav.com>

RUN apk --update --no-cache add htop && \
	rm -rf /var/lib/apt/lists/*

ENTRYPOINT ["htop"]
