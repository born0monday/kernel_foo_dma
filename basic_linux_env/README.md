# basic_linux_env
Sets up a basic linux env to test kernel images

# Setup
This basic environment sets up a basic initramfs with a host directory mounted into the linux env the kernel boots into

# Steps
1. Init git submodules and build initramfs
2. Copy your bzImage kernel image into the root directory (Make sure 9p is enabled in the kernel)
3. Create ./host directory that'll be mounted in guest at /mnt/host
3. Run ./run_qemu.sh