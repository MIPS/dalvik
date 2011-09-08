JAVAC=javac
JAR=jar
JVM=java

run: Main.jar
	$(JVM) -cp Main.jar Main

Main.jar: Main.class
	$(JAR) cf Main.jar Main.class

Main.class: Main.java
	$(JAVAC) Main.java
