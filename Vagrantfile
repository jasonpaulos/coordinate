Vagrant.configure("2") do |config|
  config.vm.box = "ubuntu/bionic64"

  config.vm.network "forwarded_port", guest: 8000, host: 8000
  config.vm.network "forwarded_port", guest: 26100, host: 26100, host_ip: "127.0.0.1"

  # synced folder
  config.vm.synced_folder '.', '/coordinate'

  # disable default synced folder
  config.vm.synced_folder '.', '/vagrant', disabled: true

  config.vm.provision "shell", inline: <<-SHELL
    apt-get update
    apt-get install -y build-essential gdbserver
  SHELL
end
