# meson.build patch for hw/display/ in QEMU

# Add this to the appropriate section of hw/display/meson.build
# softmmu_ss.add(when: 'CONFIG_GRAVITY_GPU', if_true: files('gravity-gpu.c', 'gravity-gpu-pci.c'))

system_ss.add(when: 'CONFIG_GRAVITY_GPU', if_true: files(
  'gravity-gpu.c',
  'gravity-gpu-pci.c'
))
