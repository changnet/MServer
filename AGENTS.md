当前项目的规则、知识库在.agnets目录，在执行操作前，按需读取里面的文件

.agents
	rules 规则目录
		code.md 编码规则，写代码前必读
	docs 知识库
	skills 技能

如果读取了其中的文件，一定要列出读取了哪些文件，如
```
已读取
	docs/architecture.md
	rules/code.md
```

.agents目录上传到git，除非有明确要求，否则agent工作过程中不允许把任何文件写入.agents目录
