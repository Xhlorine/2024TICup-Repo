## FFT幅度优化方案

相邻最大两个值，设谐波次数为$i_1,i_2$，幅度为$m_1>m_2$，则真正的峰值$M$在$i_1$与$i_2$之间，且与$i_1$距离为$d$的点上。
$$
M\times sinc(d) = m_1\\
M\times sinc(1-d) = m_2\\
d = \frac{m_2}{m_1+m_2}\\
M = \frac{m_1}{sinc(\frac{m_2}{m_1+m_2})}
$$

实测结果精确度大大提升，Very Good。

## FFT采样优化方案<del>(存疑)</del>

$$
T_s=(K+\frac{1}{N})T\\
f=(K+\frac{1}{N})f_s\\
\text{When } K=0\text{, }f_s=Nf\text{. Here }N=\text{NPT/baseIndex}\\
\text{Now }\frac{N}{1+NK}=\frac{T}{T_s}=\frac{f_s}{f}\\
\text{Since }f\text{ has been calculated before, adjust that }
f_s=f\bullet\frac{N}{1+NK}
$$

个人感觉，一般N起码有十几，很难把$f_s$控制的这么精准吧，<del>实践意义不大？</del>考虑外置DDS作为ADC采样时钟的话还是挺准确的

> 后来也采用了，效果还是可以的，在低采样频率下stm32可以做到$f_s$的精度在0.3Hz以内，基本是够用的
