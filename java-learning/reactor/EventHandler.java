/**
 * @Author: feiweiwei
 * @Description: event处理器的抽象类
 * @Created Date: 11:26 17/10/12.
 * @Modify by:
 */
public abstract class EventHandler {

    private InputSource source;

    public abstract void handle(Event event);

    public InputSource getSource() {
        return source;
    }

    public void setSource(InputSource source) {
        this.source = source;
    }
}