---
date: 2023/04/21 15:49:27
updated: 2024/05/24 16:18:56
---

# system-architect

- [💯2024年 系统架构设计师（软考高级）备考资源库+配套免费刷题软件。](https://github.com/xiaomabenten/system_architect)
- [软考达人](https://ruankaodaren.com/)

## 软件开发模型

- [软件工程基础 - 九种开发模型](https://blog.csdn.net/xc917563264/article/details/115024764)
- [统一软件过程的基本概念](https://blog.csdn.net/shadowyelling/article/details/7412336)
- [浅谈RUP的9个核心工作流(Core Workflows)](https://blog.51cto.com/tonyguan/712491)
- [开发方法-基于架构的软件设计](https://blog.csdn.net/hu19930613/article/details/82154842)

## 需求分析-用例图

- [包含、扩展、泛化的区别](https://blog.csdn.net/wrs120/article/details/52838469)
- [用例关系（包含、扩展、泛化） uml类关系（依赖、关联、聚合、组合、泛化）](https://blog.csdn.net/abcd1101/article/details/83240965)
- [终于明白六大类UML类图关系了](https://segmentfault.com/a/1190000021317534)

- [软件需求3个层次――业务需求、用户需求和功能需求](https://blog.51cto.com/u_15067225/3989277)

- [1分钟了解流程图、顺序图、状态图](https://blog.csdn.net/Edraw_Max/article/details/111474777)

## 软件架构风格

- [C/S结构的两层模型、三层模型及多层模型](http://blog.itpub.net/8816263/viewspace-1006692/)

- [WebService,ESB笔记](https://www.cnblogs.com/applerosa/p/6001270.html)

## 构件与中间件技术

- [“构件”和“对象”](https://blog.csdn.net/wishfly/article/details/2026340)

## 项目管理

- [项目管理中的WBS应该怎么做](https://www.jianshu.com/p/1b0305165d9e)
- [工作分解结构(Work Breakdown Structure，简称WBS)](https://wiki.mbalib.com/wiki/%E5%B7%A5%E4%BD%9C%E5%88%86%E8%A7%A3%E7%BB%93%E6%9E%84)

## 应用数学

- [甲、乙、丙、丁4人加工A、B 、C、D四种工件所需工时如下表所示。指派每人加工一种工件，四人加工四种工件其总工时最短的最优方案中，工件B应由（ ）加工。](https://wx.xisaiwang.com/tiku2/85220.html)

## SOA

面向服务的架构 (SOA) 是一种设计模式，用于构建灵活、可重用的企业系统。SOA 的核心概念是将业务功能模块化为独立的服务，这些服务可以通过网络进行通信和集成。在 SOA 中，Web Service 和企业服务总线 (ESB) 是两个关键组件，它们在实现 SOA 的过程中发挥着重要作用，但它们的职责和功能有显著区别。

### Web Service

**Web Service (网络服务)** 是一种标准化的方式，用于在不同的应用程序之间通过网络进行通信和数据交换。Web Service 主要依赖于一些标准和协议，如 SOAP（Simple Object Access Protocol）、WSDL（Web Services Description Language）和 UDDI（Universal Description, Discovery, and Integration）。

#### 主要特点：
1. **标准化协议**：Web Service 使用标准化协议（如 HTTP、SOAP）进行通信，确保不同平台和编程语言之间的互操作性。
2. **松耦合**：服务提供者和服务消费者之间是松耦合的，服务接口通过 WSDL 描述，这使得服务可以独立地演进和更新。
3. **可重用性**：通过将功能封装成服务，Web Service 可以被多个应用程序重用。
4. **可发现性**：使用 UDDI 注册和发现服务，使得服务的查找和使用变得更容易。

#### 用途：
- 不同应用程序和系统之间的数据交换。
- 提供标准化的服务接口，供内部或外部系统使用。
- 实现跨平台和跨语言的系统集成。

### ESB (Enterprise Service Bus)

**企业服务总线 (ESB)** 是一种中间件架构，旨在促进复杂企业环境中各种服务之间的通信、集成和协调。ESB 提供了一个高度可扩展的基础设施，用于管理服务之间的交互和数据传输。

#### 主要特点：
1. **消息路由**：ESB 能够根据预定义的规则路由消息到适当的服务。
2. **协议转换**：ESB 支持多种通信协议，并能够在不同协议之间进行转换（如 HTTP 到 JMS）。
3. **数据转换**：ESB 可以对消息数据进行格式转换，以确保不同服务之间的数据兼容性。
4. **服务编排**：通过工作流或业务流程管理 (BPM) 引擎，ESB 能够编排和管理多个服务的交互。
5. **监控和管理**：ESB 提供监控和管理功能，帮助跟踪服务调用和性能。

#### 用途：
- 集成不同的应用程序和系统（如 ERP、CRM）。
- 管理复杂的服务交互和业务流程。
- 实现集中化的服务监控和管理。
- 提供一个统一的通信平台，简化企业系统架构。

### 区别总结

| 特点 | Web Service | ESB |
| --- | --- | --- |
| **定义** | 一种标准化的方式，用于不同应用程序之间进行通信和数据交换 | 一种中间件架构，促进企业内各种服务之间的通信、集成和协调 |
| **协议** | 主要使用 HTTP、SOAP、REST | 支持多种协议（如 HTTP、JMS、MQ）并进行协议转换 |
| **作用** | 提供标准化服务接口，实现跨平台、跨语言的数据交换 | 管理服务间的通信、消息路由、数据转换、服务编排和监控 |
| **用途** | 服务接口标准化和互操作性 | 企业级系统集成和复杂服务交互管理 |
| **实现复杂性** | 较简单，适用于独立服务的发布和调用 | 较复杂，适用于多服务集成和协调 |

### 示例

- **Web Service 示例**：一个库存管理系统通过 Web Service 接口公开其库存查询服务，其他系统可以通过调用这个 Web Service 来获取库存信息。

- **ESB 示例**：一个零售企业的订单处理系统使用 ESB 来集成多个服务，包括库存检查、支付处理和物流安排。ESB 负责在这些服务之间路由消息、转换数据格式，并确保整个订单处理流程的协调和监控。

通过 Web Service 和 ESB 的组合，企业能够实现灵活、高效和可扩展的系统集成和服务管理，充分发挥 SOA 的优势。