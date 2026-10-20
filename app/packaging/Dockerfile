# Install the latest version of ubuntu
FROM ubuntu:latest

# Update packets
RUN apt-get update

# Install mc
RUN apt install -y mc

# Sudo is not installed on docker by default
RUN apt install -y sudo

# --------------------------- INSTALL PS3 SDK

# Download ps3dev
RUN cd /usr/local && wget https://github.com/Fewnity/Xenity-Engine-SDK/releases/download/v1/ps3dev-ubuntu-latest.tar.gz
# Extract files
RUN cd /usr/local && tar -xvzf ps3dev-ubuntu-latest.tar.gz && rm ps3dev-ubuntu-latest.tar.gz

# Download PSL1GHT
RUN cd /usr/local && wget https://github.com/Fewnity/Xenity-Engine-SDK/releases/download/v1/PSL1GHT.tar.gz
# Extract files
RUN cd /usr/local && tar -xvzf PSL1GHT.tar.gz && rm PSL1GHT.tar.gz

# Install nvidia shader compiler
RUN apt install -y nvidia-cg-toolkit

RUN echo "export PS3DEV=/usr/local/ps3dev" >> /root/.bashrc
RUN echo "export PATH=\$PATH:\$PS3DEV/bin" >> /root/.bashrc
RUN echo "export PATH=\$PATH:\$PS3DEV/ppu/bin" >> /root/.bashrc
RUN echo "export PATH=\$PATH:\$PS3DEV/spu/bin" >> /root/.bashrc
RUN echo "export PSL1GHT=/usr/local/PSL1GHT/build" >> /root/.bashrc
RUN echo "export PATH=\$PATH:/usr/local/PSL1GHT/tools/cgcomp" >> /root/.bashrc

# --------------------------- Create build folders
RUN cd /home/ && mkdir XenityBuild && cd XenityBuild && mkdir Source && mkdir build && mkdir include && mkdir shaders_to_compile

# Clean temp files
RUN rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Default command
CMD ["/bin/bash"]

# To run a image to check if everything is ok, open a cmd and run: docker run -it ubuntu_test