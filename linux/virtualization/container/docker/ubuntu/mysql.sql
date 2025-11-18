use mysql;
ALTER USER 'root'@'localhost' IDENTIFIED WITH mysql_native_password BY '2988'; 
UPDATE user SET host = '%' WHERE user = 'root';
flush privileges;