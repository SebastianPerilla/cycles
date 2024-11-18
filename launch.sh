export CYCLES_PORT=3101

cat<<EOF> config.yaml
gameHeight: 800
gameWidth: 1000
gameBannerHeight: 100
gridHeight: 100
gridWidth: 100
maxClients: 60
enablePostProcessing: false
EOF

/path/to/server &
SERVER_PID=$!

sleep 1

for i in {1..4}
do
  /path/to/client randomio$i &
done

/path/to/client_sebastian_perilla sebastian_perilla