在虚幻5里面如果想在Runtime中生成模型并显示，那UE有几个自带的组件都可以生成模型。

其中就有 UStaticMeshComponent（SMC）、UProceduralMeshComponent（PMC）、UDynamicMeshComponent（DMC）。

在其他很多文章中对这3个都有比较，大部分都是说DMC绘画效率比PMC绘画效率高，但是在我实例测试中，效率 SMC > PMC > DMC。 SMC是绘画最快的，DMC是绘画最慢的。

在模型所有数据的相同的情况下：点数量 1442401， 面数量 2880000

SMC 生成时间 16秒， 绘画GPU时间12毫秒
![UStaticMeshComponent测试效果](https://dt.cq.cn/wp-content/uploads/2023/07/image-2.png "UStaticMeshComponent测试效果")

PMC 生成时间 4秒， GPU时间21毫秒
![UProceduralMeshComponent测试效果](https://dt.cq.cn/wp-content/uploads/2023/07/image-3.png "UProceduralMeshComponent测试效果")

DMC 生成时间 5秒， GPU时间24毫秒
![UDynamicMeshComponent测试效果](https://dt.cq.cn/wp-content/uploads/2023/07/image-4.png "UDynamicMeshComponent测试效果")

并且PMC和DMC在计算碰撞的时候都可以开启异步计算，如果开启异步计算碰撞,PMC的生成时间只有0.2秒，而DMC还是需要1.2秒。

也就说明DMC不管是生成时间还是绘画效率都没有PMC块，DMC唯一的好处就是UE官方出了很多操作函数，可自定义性，可操作性比PMC强很多。

本测试是基于 5.3.2 版本测试。

[原始文章](https://dt.cq.cn/archives/89)