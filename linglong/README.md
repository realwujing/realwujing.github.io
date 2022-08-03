# k8s部署玲珑服务

## 创建namespace

```bash
kubectl create ns linglong
```

## linglong-homepage

```bash
docker build -t hub.deepin.com/wuhan_v23_linglong/linglong-homepage:develop-snipe .
```

```bash
docker run --rm -it -p 18080:80/tcp hub.deepin.com/wuhan_v23_linglong/linglong-homepage:develop-snipe
```

```bash
kubectl create -n linglong deployment linglong-homepage --image=hub.deepin.com/wuhan_v23_linglong/linglong-homepage:develop-snipe
```
<!-- 
```bash
kubectl get deployments.apps linglong-homepage -o yaml > linglong-homepage.yaml
```

```bash
kubectl delete -f linglong-homepage.yaml
```

```text
vim linglong-homepage.yaml 将 namespace 从 default改为 linglong
```

```bash
kubectl apply -f linglong-homepage.yaml
``` -->

- 使用 18080 端口提供服务，连接到容器的 80 端口

```bash
kubectl expose -n linglong deployment linglong-homepage --type=NodePort --port=18080 --target-port=80
```

## linglong-webstore

```bash
docker build -t hub.deepin.com/wuhan_v23_linglong/linglong-webstore:develop-snipe .
```

```bash
docker run --rm -it  -p 18081:80/tcp hub.deepin.com/wuhan_v23_linglong/linglong-webstore:develop-snipe
```

```bash
kubectl create -n linglong deployment linglong-webstore --image=hub.deepin.com/wuhan_v23_linglong/linglong-webstore:develop-snipe
```

<!-- ```bash
kubectl get deployments.apps linglong-webstore -o yaml > linglong-webstore.yaml
```

```bash
kubectl delete -f linglong-webstore.yaml
```

```text
vim linglong-webstore.yaml 将 namespace 从 default改为 linglong
```

```bash
kubectl apply -f linglong-webstore.yaml
``` -->

- 使用 18081 端口提供服务，连接到容器的 80 端口

```bash
kubectl expose -n linglong deployment linglong-webstore --type=NodePort --port=18081 --target-port=80
```

## linglong-server

- 创建configmap

```bash
kubectl create configmap config.yaml --from-file=config.yaml -n linglong
```

- 创建pv

```bash
kubectl apply -f linglong-server-pv.yaml
```

- 创建pvc

```bash
kubectl apply -f linglong-server-pvc.yaml
```

- linglong-server部署

```bash
docker build -t hub.deepin.com/wuhan_v23_linglong/linglong-server:develop-snipe .
```

```bash
docker run --rm -it  -p 18888:8888/tcp hub.deepin.com/wuhan_v23_linglong/linglong-server:develop-snipe
```

```bash
kubectl create -n linglong deployment linglong-server --image=hub.deepin.com/wuhan_v23_linglong/linglong-server:develop-snipe
```

```bash
kubectl get deployments.apps linglong-server -o yaml > linglong-server.yaml
```

```bash
kubectl delete -f linglong-server.yaml
```

```text
vim linglong-server.yaml 更改volumeMounts、volumes节点，挂载linglong-server-pvc
```

```bash
kubectl apply -f linglong-server.yaml
```

- 使用 18888 端口提供服务，连接到容器的 8888 端口

```bash
kubectl expose -n linglong deployment linglong-server --type=NodePort --port=18888 --target-port=8888
```
