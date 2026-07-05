#include "frame.h"
#include <QListWidget>
#include <QDebug>
#include <QBuffer>
#include <QPainter>
#include <QFont>
#include "uiItems.h"
#define CS currentStickFigure->currentStick
QListWidget * ListWidget;
extern QListWidget * myStickFigureWidgetList;
extern bool undoFlag;
extern bool libFlag;
extern bool switchFrameFlag;

bool iconUpdateFlag = false;
Frame::Frame()
{
    frameIcon       = new QIcon();
    iconImg         = new QPixmap(50,50);
    currentStickFigure = nullptr;
}
Frame::~Frame(){
    for(StickFigure* S: stickFigures)
    {
        delete S;
    }
    delete iconImg;
    delete frameIcon;
    //delete renderImg;
}
extern bool copyFlag;
StickFigure* Frame::addStickFigure(QListWidget * myListWidget, QString name)
{
    QString myName;
    if(!libFlag){
        ListWidget = myStickFigureWidgetList;
    }
    else{
        ListWidget = myCurrentLibraryWidget;
    }

    if(!libFlag && !copyFlag){

        if(name.count()== 0){
            int intName = 0;
            if(!stickFigures.isEmpty()){
                intName = myStickFigureWidgetList->currentRow()+1;
            }

            myName.asprintf("%d",intName);
        }
        else{
            myName = name;
        }
    }
    //deseleziona graficamente nel viewport

    QListWidgetItem * addedItem = new QListWidgetItem();

    //StickFigure* addedStickFigure = view->addStickFigure( addedItem);

    if(currentStickFigure!=nullptr)
    {
        if(!stickFigures.isEmpty() && !currentStickFigure->stickList.isEmpty())
        {
            currentStickFigure->highlight(false); //de highlight old selection
        }
    }
    stickFigureBuffer = new StickFigure(addedItem);
    stickFigures.append(stickFigureBuffer);
    //seleziona l'ordine z in base all'ordine di creazione
    stickFigureBuffer->baseZ = stickFigures.count();
    //aggiorna lo stickfigure corrente
    currentStickFigure = stickFigureBuffer;
    currentStickFigure->name = myName;
    if(!undoFlag && !libFlag &&!copyFlag){
        QVariant newData(QVariant::fromValue(currentStickFigure));
        addedItem->setData(Qt::UserRole,newData);
        addedItem->setData(Qt::DisplayRole,myName);
        addedItem->setIcon(*currentStickFigure->stickFigureIcon);
        myListWidget->insertItem(myListWidget->currentRow()+1,addedItem);
    }
    // update current stickfigure
    if(!libFlag && !copyFlag){
        currentStickFigure->highlight(false); //de highlight old selection
        currentStickFigure->highlight(true); //highlight new one
    }
    if(!undoFlag && !switchFrameFlag && !copyFlag){
        // update current stick
        if(!libFlag)
        {
            scene->clearSelection(); //clear scene selection
            if(!currentStickFigure->stickList.isEmpty())
            {
                selectStick(currentStickFigure); // puo essere ridontante in quanto ci sta la callback di sticklayerView_currentItemChanged//update selected stick
            }
        }
        int tempIndex = myListWidget->currentRow();
        myListWidget->clearSelection();
        addedItem->setSelected( true);
        myListWidget->setCurrentRow(tempIndex+1);
    }
    currentStickFigure->scene = scene;

    return stickFigureBuffer;
}
StickFigure* Frame::addStickFigureToLibrary(StickFigure*S){
    if(S != nullptr){
        QListWidgetItem * addedItem = new QListWidgetItem();
        //stickFigureBuffer = addStickFigure(myCurrentLibraryWidget, S->name);
        StickFigure *newStickFigure =new StickFigure();
        newStickFigure->linkedItem = addedItem;
        stickFigures.append(newStickFigure);
        // byte array stores serialized data
        QByteArray* byteArray = new QByteArray();
        // buffer temporarily holds serialized data
        QBuffer buffer1(byteArray);
        // use this buffer to store data from the object
        buffer1.open(QIODevice::WriteOnly);
        QDataStream myStream(&buffer1);
        myStream<<*(S);
        // now create a seconds buffer from which to read data of the bytearray
        QBuffer buffer2(byteArray);
        buffer2.open(QIODevice::ReadOnly);
        // a new data stream to deserialize
        QDataStream myStream2(&buffer2);
        // hydrate new frame with previous frame data
        myStream2>>*newStickFigure;


        QVariant newData(QVariant::fromValue(newStickFigure));

        addedItem->setData(Qt::UserRole,newData);
        addedItem->setData(Qt::DisplayRole,newStickFigure->name);
        int newIdx = myCurrentLibraryWidget->currentRow()+1;
        myCurrentLibraryWidget->insertItem(newIdx,addedItem);

        myCurrentLibraryWidget->clearSelection();
        addedItem->setSelected( true);
        myCurrentLibraryWidget->setCurrentRow(newIdx);
        // era S->updateIcon(): aggiornava l'icona dello stickfigure sorgente invece di quella del
        // clone appena aggiunto alla libreria, che restava senza icona finché la libreria non
        // veniva salvata e ricaricata da file (updateIcon() imposta anche addedItem->setIcon(...))
        newStickFigure->updateIcon();
        delete byteArray;
        return newStickFigure;
    }
    return nullptr;
}
StickFigure* Frame::removeStickFigure(StickFigure* toRemove)
{
    if(stickFigures.count() > 0)
    {
        stickFigures.removeAll(toRemove);
        delete toRemove;
        if(!stickFigures.isEmpty())
            currentStickFigure = stickFigures[0];
        else
        {
            qDebug("removed all sticks");
            currentStickFigure = nullptr;
        }
        stickFigureBuffer = currentStickFigure;

    }
    return currentStickFigure;
}
stick *Frame::selectStick(StickFigure* S, stick* s)
{
    int stickIdx = 0;
    if(s!=nullptr){
        if(S->stickList.contains(s))
            stickIdx = S->stickList.indexOf(s);
    }
    S->highlight(true);
    S->stickList[stickIdx]->setSelected(true);
    S->currentStick = S->stickList[stickIdx];
    S->currentStick->selected = true;

    return S->stickList[stickIdx];
}
void Frame::refresh(int mode){
    for(stick*s:totalSticks){
        scene->removeItem(s);
    }
    for(stick*s:totalSticks){
        scene->addItem(s);
    }
    for(StickFigure*S:stickFigures){
        S->refresh(mode);
    }
}
stick *Frame::selectStick(QPointF point)
{
    float distBuffer;
    float minDist;
    int idxBuffer;
    int selectedIdx = -1;
    StickFigure *selectedStickFigure;
    for(StickFigure * S: stickFigures)
    {
        // su un criterio di distanza dall'handle utile, scegli l'item stick più vicino per ogni stickfigure
            idxBuffer = S->selectStick( &point);
            distBuffer = QLineF(S->stickList[idxBuffer]->myLine.p2(),point).length();
            //scegli il più vicino tra tutti gli stickfigure
            if(selectedIdx == -1 || (distBuffer<minDist))
            {
                selectedIdx = idxBuffer;
                minDist = distBuffer;
                selectedStickFigure = S;
            }
    }
    selectedStickFigure->stickList[selectedIdx]->setSelected(true); //for scene
    selectedStickFigure->currentStick->selected = true; //for rendering purpose
    return selectedStickFigure->stickList[selectedIdx];
}
QDataStream & operator<< (QDataStream& stream, const Frame& myFrame){
    int num = (int)myFrame.stickFigures.count();
    stream<< num ;
    stream<<myFrame.numOfItems;
    stream<< myFrame.frameNumber;
    stream<< (int)myFrame.stickFigures.indexOf(myFrame.currentStickFigure);
    qDebug() << "before streaming sf";
    if(!libFlag){
        if(myFrame.currentStickFigure)
        {
            stream<< (int)myFrame.currentStickFigure->stickList.indexOf(myFrame.currentStickFigure->currentStick);
        }else{
            stream<<0;
        }
    }
    for(StickFigure* S: myFrame.stickFigures){
        stream<< *S;
    }
    qDebug() << "after stream sf";
    return stream;
}
QDataStream & operator>> (QDataStream& stream, Frame& myFrame){
    int max = 0;
    int currentStickIdx = 0;
    int currentStickFigureIdx = 0;
    stream>> max;

    stream>>myFrame.numOfItems;
    stream>> myFrame.frameNumber;
    stream>>currentStickFigureIdx;
    if(!libFlag){
        stream>>currentStickIdx;
    }else{
        int buff;
        stream>>buff;
    }


    myFrame.totalSticks.clear();
    QListWidget * Widget;
    if(libFlag) {
        Widget = myCurrentLibraryWidget;
    }
    else{
        Widget = myStickFigureWidgetList;
    }

    for(int i = 0;i<max;i++){

        StickFigure* S = myFrame.addStickFigure(Widget);
        stream>>*S;
        S->linkedItem->setData(Qt::DisplayRole,S->name);
        myFrame.totalSticks.append(S->stickList);
        //myFrame.stickFigures.append(S);
    }

    for(StickFigure*S:myFrame.stickFigures){
        for(stick*s:S->stickList){
            myFrame.totalSticks[s->order] = s;
        }
    }
    myFrame.currentStickFigure = nullptr;
    if(currentStickIdx>=0 && currentStickFigureIdx>=0 && currentStickFigureIdx<myFrame.stickFigures.length()){
        if(currentStickIdx < myFrame.stickFigures[currentStickFigureIdx]->stickList.length()){
            myFrame.currentStickFigure = myFrame.stickFigures[currentStickFigureIdx];
            myFrame.currentStickFigure->currentStick = myFrame.currentStickFigure->stickList[currentStickIdx];
        }
    }
    return stream;
}
float Frame::minMaxZ(int option){
    if(totalSticks.isEmpty())
        return 0;
    float peak = totalSticks[0]->Z;
    for(stick*s:totalSticks){
        if(option){
            if(s->Z>peak)
                peak = s->Z;
        }
        else{
            if(s->Z<peak)
                peak = s->Z;
        }
    }
    return peak;
}
void Frame::stickfigureToBottom(StickFigure* S){
    S->updateZ(minMaxZ(0));
    int min = -1;
    for(int j = totalSticks.length()-1;j>min;j--){
        if(S->stickList.contains(totalSticks[j])){
            totalSticks.move(j,0);
            min++;
            j++;
        }
    }

}
void Frame::stickfigureToTop(StickFigure* S){
    S->updateZ(minMaxZ(1));
    int max = totalSticks.length()-1;
    for(int j = 0; j<max;j++){
        if(S->stickList.contains(totalSticks[j])){
            totalSticks.move(totalSticks.indexOf(totalSticks[j]),max);
            j--;
            max--;
        }
    }

}

void Frame::stickfigureAboveBelow(StickFigure* toMove, StickFigure *previous, int mode){

    mode == 1?toMove->updateZ(previous->maxZ()): toMove->updateZ(previous->minZ());
    int peakIdx = 0;
    QList<stick*> sticksToMove;
    if(mode == 1){
        for(int j = totalSticks.length()-1; j >-1 ;j--){
            if(previous->stickList.contains(totalSticks[j])){
                peakIdx = j; // il più alto che deve superare toMove
                break;
            }
        }
        for(stick*s:toMove->stickList){
            if(totalSticks.indexOf(s)<peakIdx)
                sticksToMove.append(s);
        }
        for(stick*s:sticksToMove){
            totalSticks.move(totalSticks.indexOf(s),peakIdx);
        }
    }
    else{
        for(int j = 0; j<totalSticks.length();j++){
            if(previous->stickList.contains(totalSticks[j])){
                peakIdx = j; // il piu basso, sotto al quale si deve abbassare il toMove
                break;
            }
        }
        for(stick*s:toMove->stickList){
            if(totalSticks.indexOf(s)>peakIdx)
                sticksToMove.prepend(s);
        }
        for(stick*s:sticksToMove){
            totalSticks.move(totalSticks.indexOf(s),peakIdx);
        }
    }


}
// questa funzione aggiorna l'iconea del livello
void Frame::updateIcon()
{
    iconUpdateFlag = true;
    //crea una scena fittizia, falla bianca e delle stesse dimensioni di quella principale
    QGraphicsScene icoScene;
    iconImg->fill(Qt::white);
    icoScene.setBackgroundBrush(QBrush(QColor(Qt::white)));
    icoScene.setSceneRect(scene->sceneRect());
    // ora clona tutti gli stick nello stickfigure e aggiungili alla scena fittizia
    // (addItem rende la scena proprietaria dei cloni: non vanno cancellati a mano,
    // altrimenti verrebbero distrutti due volte quando icoScene esce di scope)
    for(StickFigure *S:stickFigures)
    {
        for(stick* s: S->stickList){
            icoScene.addItem(new stick(s));
        }
    }
    // crea un painter che disegni sull'immagine
    QPainter painter(iconImg);
    // renderizza dalla scena sull'immagine mediante il painter
    icoScene.render(&painter);
    // disegna il numero del frame direttamente sull'iconea (fascia scura semi-trasparente in basso,
    // testo bianco sopra): affidarsi all'etichetta nativa della QListWidgetItem in IconMode non
    // mostrava il numero in modo affidabile, quindi lo "cuociamo" nell'immagine stessa
    QRectF numberBand(0, iconImg->height()-14, iconImg->width(), 14);
    painter.fillRect(numberBand, QColor(0,0,0,160));
    painter.setPen(Qt::white);
    QFont numberFont = painter.font();
    numberFont.setPointSize(8);
    painter.setFont(numberFont);
    painter.drawText(numberBand, Qt::AlignCenter, QString::number(frameNumber));
    // imposta l'iconea
    delete frameIcon;
    frameIcon = new QIcon(*iconImg);
    linkedItem->setIcon(*frameIcon);
    linkedItem->setData(Qt::DisplayRole, QString()); // il numero è già nell'iconea: evita di mostrarlo due volte

    // i cloni vengono cancellati automaticamente dal distruttore di icoScene
    painter.end();
    iconUpdateFlag = false;
}

void Frame::updateRender(){
    /*QGraphicsScene renderScene;
    renderScene.setSceneRect(scene->sceneRect());
    // ora clona tutti gli stick nello stickfigure in una tempList, aggiungili alla scena fittizia
    QList<stick*> tempList;
    for(StickFigure *S:stickFigures)
    {
        for(stick* s: S->stickList){
            stick* clone = new stick(s);
            tempList.append(clone);
            renderScene.addItem(clone);
        }
    }
    QPainter painter(renderImg);
    renderImg->fill(Qt::transparent);
    painter.setBackground(QBrush(QColor(Qt::transparent)));
    renderScene.render(&painter);
    renderScene.clear();*/
}
void Frame::clearFrame(){
    for(StickFigure *S:stickFigures){
        // il frame viene svuotato per sempre qui: libera gli stick esplicitamente invece di
        // affidarsi al distruttore di StickFigure, che salta la pulizia quando undoFlag/copyFlag/
        // libFlag/clearUndoFlag sono attivi (pensati per gli oggetti "di passaggio" della
        // (de)serializzazione, non per una cancellazione definitiva come questa - senza questo,
        // ogni undo/redo che passa da qui perdeva la memoria del frame sovrascritto)
        for(stick* s: S->stickList){
            scene->removeItem(s);
            delete s;
        }
        delete S;

    }
    stickFigures.clear();

}

void Frame::normalizeDepth()
{


}
void Frame::cleanTotalSticks(void){
    QList<stick*> temp;
    for(StickFigure* S:stickFigures){
        for(stick *s:S->stickList){
            if(!totalSticks.contains(s)){
                totalSticks.insert(s->order,s);
                temp.append(s);

            }
        }
    }
    for(stick*s:totalSticks){
        if(!temp.contains(s)){
            totalSticks.removeAll(s);
        }
    }
}
