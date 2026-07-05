#include "StickFigure.h"

#include <QBuffer>
#include <QDebug>
#include <QFile>
#include <QVector2D>
// StickFigure work in the one active scene
extern bool loadingAnimationFlag;
extern bool undoFlag;
extern bool libFlag;
extern bool clearUndoFlag;
extern bool copyFlag;
extern bool iconUpdateFlag;
extern QGraphicsRectItem* myRect;
StickFigure::~StickFigure()
{
    if(!undoFlag && !libFlag && !clearUndoFlag && !copyFlag){
        for(stick* s: stickList)
        {
            //if(!clearUndoFlag)
            scene->removeItem(s);
            delete s;
        }
    }
    // già vengono distrutti dal clear della libreria
    if(!libFlag){
        delete iconImg;
        delete stickFigureIcon;
    }
}
// questa funzione aggiorna l'iconea del livello
void StickFigure::updateIcon()
{
    iconUpdateFlag = true;
    //crea una scena fittizia, falla bianca e delle stesse dimensioni di quella principale
    QGraphicsScene icoScene;
    iconImg->fill(Qt::white);

    if(!libFlag && !copyFlag)
        icoScene.setSceneRect(scene->sceneRect());
    else
        icoScene.setSceneRect(myRect->rect());
    icoScene.setBackgroundBrush(QBrush(QColor(Qt::white)));
    // ora clona tutti gli stick nello stickfigure e aggiungili alla scena fittizia
    // (addItem rende la scena proprietaria dei cloni: non vanno cancellati a mano,
    // altrimenti verrebbero distrutti due volte quando icoScene esce di scope)
    for(stick* S: stickList){
        icoScene.addItem(new stick(S));
    }
    // crea un painter che disegni sull'immagine
    QPainter painter(iconImg);
    // renderizza dalla scena sull'immagine mediante il painter
    icoScene.render(&painter);
    // imposta l'iconea
    delete stickFigureIcon;
    stickFigureIcon = new QIcon(*iconImg);
    linkedItem->setIcon(*stickFigureIcon);
    // i cloni vengono cancellati automaticamente dal distruttore di icoScene
    iconUpdateFlag = false;
}
StickFigure::StickFigure(QListWidgetItem* item)
{
    p0          =  QPointF(0,0);
    p1          =  QPointF(0,0);
    lineBuffer  =  QLineF(p0,p1);
    drawCount   =  0;
    masterStick =   nullptr;
    currentStick = nullptr;
    linkedItem  = item;
    stickFigureIcon = new QIcon();
    iconImg         = new QPixmap(50,50);
    masterStick = nullptr;
}
StickFigure::StickFigure()
{
    p0          =  QPointF(0,0);
    p1          =  QPointF(0,0);
    lineBuffer  =  QLineF(p0,p1);
    drawCount   =  0;
    masterStick =   nullptr;
    currentStick = nullptr;
    stickFigureIcon = new QIcon();
    iconImg         = new QPixmap(50,50);
    masterStick = nullptr;
}
//inizio disegno di una linea stick
void StickFigure::startDrawing(QPointF *point, QPen pen, QBrush brush)
{
    if(stickList.isEmpty()) // se è il primo stick dello stickfigure
    {
        p0 = *point;
    }
    else //se ci sono altri stick in questo stickfigure i dalla estremità più vicinaz al click
    {
        parentBuffer = stickList[selectOrigin(point)]; //seleziona genitore (potremmo già farlo al rigo precedente visto che la funzione viene chiamata)
        if(selectingOrigin)
            p0 = parentBuffer->myLine.p1();
        else
            p0 = parentBuffer->myLine.p2();
    }

    //crea e alloca l'oggetto stick associato
    lineBuffer      = QLineF(p0,p0); // all'inizio la linea ha lunghezza zero
    stick *stk      = new stick(&lineBuffer);
    stk->myStickFigure = this;
    //assegna l'ordine di profondità
    stk->Z = baseZ+stickList.count()/100;
    stickBuffer     = stk;
    currentStick    = stk;
    // seè il primo stick ad essere disegnato
    if(this->masterStick == nullptr){
        masterStick = currentStick;
        masterStick->stickHandle[0]->setBrush(QBrush(Qt::green));
        masterStick->stickHandle[0]->setZValue(101);
        //masterStick->master = true;
        masterStick->parent = nullptr;
    }
    drawCount       = 1; // segnala che un disegno è in atto
    stickBuffer->Pen = pen;
    stickBuffer->Brush = brush;
    stickBuffer->refresh(0);
}
// aggiorna uno stick il cui disegno è già avviato
void StickFigure::previewDrawing(QPointF *point)
{
    setLineFromPoint(point);
    stickBuffer->refresh(1);
    if(stickBuffer->stickType == stick::IMAGE ||
            stickBuffer->stickType == stick::RECT||
            (stickBuffer->stickType == stick::RECT)||
            (stickBuffer->stickType == stick::TRAPEZOID)||
            (stickBuffer->stickType == stick::CIRCLE)||
            (stickBuffer->stickType == stick::TAPER)){
        stickBuffer->imgAngle= -atan2(stickBuffer->myLine.dx(),stickBuffer->myLine.dy())*180/M_PI;
    }
}
void StickFigure::refresh(int mode)
{
    for(stick* s:stickList){
        s->refresh(mode);
    }
}


void StickFigure::setLineFromPoint(QPointF *point)
{
    p1              = *point;
    lineBuffer      = QLineF(p0,p1);
    // lo stick si fonda sulla linea che connette starting point e endpoint
    stickBuffer->setLine(&lineBuffer);
}

void StickFigure::highlight(bool setting)
{
    for(stick* s:this->stickList)
    {
        s->highlight = setting;

        if(!stickList.isEmpty() && setting == false){
            s->selected = false;
        }
        //s->refresh(0);
    }

}
void StickFigure::updateZ(float value){

    for(stick* S : stickList){
        float newVal =  S->Z-baseZ+value;
        if(newVal >MAXDEPTH)
            newVal = MAXDEPTH;
        else if(newVal<MINDEPTH)
            newVal = MINDEPTH;
        S->Z = newVal;
        S->setZValue(S->Z);
    }
    baseZ = value;
}

void StickFigure::endDrawing(QPointF *point)
{
    setLineFromPoint(point);
    drawCount = 0;                  //segnala la fine
    stickList.append(stickBuffer);  //disegno finito, aggiungilo alla lista
    // assegna l'elemento padre
    stickBuffer->parent = parentBuffer;
    stickBuffer->refresh(0);
    // INSERISCILO COME FIGLIO DI TUTTA LA GERARCHIA PRECEDENTE
    while(parentBuffer){
        parentBuffer->children.append(stickBuffer);
        if(selectingOrigin || parentBuffer->stepchild){ // children of a stepchild is also a stepchild!
            stickBuffer->stepchild = true;
        }
        parentBuffer = parentBuffer->parent;
    }
    if(stickBuffer->stickType == stick::IMAGE){
        stickBuffer->imgRect = calcImgRect(stickBuffer->myLine,stickBuffer->stickImg->size());

    }
    scene->clearSelection();
    stickBuffer->setSelected(true);
}
void StickFigure::cancelDrawing()
{
    scene->clearSelection();
    if(stickBuffer==masterStick){
        masterStick = nullptr;
        currentStick = nullptr;
    }
    else{
        currentStick = parentBuffer;
    }
    delete stickBuffer;
    lineBuffer  = QLineF();
    drawCount   = 0;
    if(!stickList.isEmpty()){


        currentStick->selected = true;
        currentStick->refresh(0);
    }
}

//scegli come origine lo stick più vicino al mouse fra gli stick dello stickfigure
int StickFigure::selectOrigin( QPointF * point)
{
    //seleziona lo stick più vicino
    int idx = selectStick(point);
    // se è il master stick
    //if(stickList[idx] == masterStick)
    //{
        //controlla quale estremità è la più vicina e aseconda di quale essa sia restituisci il punto
        if(measureDistanceFromLine(*point,stickList[idx]->myLine) >
                QLineF(stickList[idx]->myLine.p1(),*point).length())
               //QLineF(stickList[idx]->myLine.p1(),*point).length()/sqrtf(stickList[idx]->myLine.length()) )
        {
            // il p1 del masterstick è un eccezione e va considerata come possibile origine
            selectingOrigin = true;
            idx = stickList.indexOf(masterStick);
            //return idx;

        }
        else{
            selectingOrigin = false;
            //return idx;
        }
     return idx;
    //}
    // se non è il master stick ignora P1
    //else{
        //stickList[idx]->myLine.p2();
        //return idx;
    //}
}
float StickFigure::measureDistanceFromLine(QPointF p, QLineF l){
    /*
    QVector2D pv(p);
    QPoint dirP = (l.p2()-l.p1()).toPoint();
    QVector2D dirV(dirP);

    return pv.distanceToLine(QVector2D(l.p1().toPoint()),dirV);
    */
    /*
    QLineF perpendicLine(p,QPointF(p.x(),0.0));
    perpendicLine.setAngle(90.0+l.angle());
    QPointF result;
    l.intersect(perpendicLine,&result);
    return QLineF(result,p).length();

    float d =fabs(-(l.p2().y()-l.p1().y())*p1.x() + (l.p2().x()-l.p1().x())*p1.y() - l.p2().x()*l.p1().y() + l.p2().y()*l.p1().x() ) /
            sqrtf(powf(l.dy(),2)+powf(l.dx(),2));
    qDebug("d = %f",d);
    return d;
 *//*
    float S = (l.length()+QLineF(l.p1(),p).length()+QLineF(l.p2(),p).length())/2;
    float A = sqrtf(S*(S-l.length())*(S-QLineF(l.p1(),p).length())*(S-QLineF(l.p2(),p).length()));
    return 2*A/l.length();
    */
    float dist = QLineF(l.p2(),p).length();
    //float wdist = dist/sqrtf(l.length());
    return dist;

}
int StickFigure::selectStick( QPointF * point)
{
    int idx         = 0;
    float  minDist  = 0;
    float currentDist = 0;
    int chosenIdx = stickList.indexOf(masterStick);
    //inizializziamo a partire dall'origine del masterStick
    minDist = measureDistanceFromLine(*point,stickList[0]->myLine);
    while(idx< stickList.length())
    {
        // i punti inizio-fine dello stick corrente
        currentDist = measureDistanceFromLine(*point,stickList[idx]->myLine);
        // se la distanza è più bassa di quella minima registrata dal luogo del mouse
        if(currentDist<= minDist)
        {

            minDist         = currentDist;
            chosenIdx       = idx;
            selectingOrigin = false;
        }
        if(stickList[idx]->isMaster()){ // todo master replacement
            currentDist =QLineF(stickList[idx]->myLine.p1(),*point).length(); //QLineF(stickList[idx]->myLine.p1(),*point).length()/sqrtf(stickList[idx]->myLine.length()) ;
            if(currentDist<= minDist)
            {

                minDist         = currentDist;
                chosenIdx       = idx;
                selectingOrigin = true;
            }
        }
        idx++;
    }
    return chosenIdx;
}
// delete selected stick and all its children, if this is the master stick delete everything
void StickFigure::deleteStick(int idx)
{
    stick *selectedStick = stickList[idx];   //oggetto stick che contiene l'oggetto grafico
    scene->removeItem(stickList[idx]); //rimuovi l'oggetto dalla scena
    if(stickList[idx]->isMaster()) //todo master replacement
        this->masterStick = nullptr;
    //per ogni figlio dell'oggetto, rimuovi gli oggetti dalla scena e cancellali
    for(stick * s : selectedStick->children){
        int childIndex          = stickList.indexOf(s);
        stick *childStick       = stickList[childIndex];   //oggetto stick che contiene l'oggetto grafico
        scene->removeItem(stickList[childIndex]); //rimuovi l'oggetto dalla scena
        stickList.removeAt(childIndex);
        stick * parent          = selectedStick->parent;
        // rimuovi a ritroso ciascuno stick figlio di quello selezionato dalle listte figli di tutti gli altri sitck presenti genitori di esso
        while(parent != nullptr )
        {
            int removefromparentIdx = parent->children.indexOf(childStick);
            parent->children.removeAt(removefromparentIdx);
            parent = parent->parent; // update parent pointer
        }
        delete childStick;
        parentBuffer = nullptr;
    }
    // epura lo stick dalla lista sticklist
    stickList.removeAll(selectedStick);
    // rimuovi lo stick selezionato dalla lista dei suoi genitori
    stick * parent = selectedStick->parent;
    while(parent != nullptr)
    {
        int removefromparentIdx = parent->children.indexOf(selectedStick);
        parent->children.removeAt(removefromparentIdx);
        parent                  = parent->parent;
    }
    delete selectedStick; // distruggi l'oggetto
    lineBuffer = QLineF(); // svuota linebuffer

}
// serializzatori e deserializzatori della classe stick
QDataStream & operator<< (QDataStream& stream, const stick& myStick){
    stream<<myStick.Z;
    stream<<myStick.version;
    stream<<myStick.stickType;
    stream<<myStick.stepchild;
    stream<<myStick.myLine;
    stream<<myStick.parentIdx;
    stream<<myStick.Pen;
    stream<<myStick.Brush;
    //stream<<myStick.isMaster(); //todo master replacement
    stream<<myStick.order;
    if((myStick.stickType == stick::CIRCLE)||(myStick.stickType == stick::IMAGE)||(myStick.stickType == stick::RECT)||(myStick.stickType == stick::TRAPEZOID)||(myStick.stickType == stick::TAPER)){
        if(myStick.stickType == stick::IMAGE){
            QSize imgSize = myStick.stickImg->size();
            stream<<imgSize;
        }
        stream<<myStick.imgAngle;
        stream<<myStick.imgHScale;
        stream<<myStick.imgWScale;
        stream<<myStick.imgOffset;
        stream<<myStick.hardTop;
        stream<<myStick.hardBottom;

        if(myStick.stickType == stick::IMAGE){
            stream<< (int)myStick.stickImgList.length();
            stream<< myStick.stickImgList.indexOf(myStick.stickImg);
            int j = 0;
            for(QImage*i:myStick.stickImgList){

                stream<<*i;
                stream<<myStick.imgNameList[j];
                j++;
            }
        }
    }

    return stream;
}
QDataStream & operator>> (QDataStream& stream, stick& myStick){
    stream>>myStick.Z;
    stream>>myStick.version;
    stream>>myStick.stickType;
    stream>>myStick.stepchild;
    stream>>myStick.myLine;
    stream>>myStick.parentIdx;
    stream>>myStick.Pen;
    stream>>myStick.Brush;
    //stream>>myStick.master; //todo master replacement
    stream>>myStick.order;
    if((myStick.stickType == stick::IMAGE)||(myStick.stickType == stick::CIRCLE)||(myStick.stickType == stick::RECT)||(myStick.stickType == stick::TRAPEZOID)||(myStick.stickType == stick::TAPER)){
        QSize mysize;
        if(myStick.stickType == stick::IMAGE){

            stream>>mysize;
        }
        stream>>myStick.imgAngle;
        stream>>myStick.imgHScale;
        stream>>myStick.imgWScale;
        stream>>myStick.imgOffset;
        stream>>myStick.hardTop;
        stream>>myStick.hardBottom;
        //stream>>bytesize;
        if(myStick.stickType == stick::IMAGE){
            int numOfImgs;
            int stickImgIdx;
            stream>>numOfImgs;
            stream>>stickImgIdx;
            for(int i = 0; i<numOfImgs;i++){
                QImage *img = new QImage(mysize,QImage::Format_ARGB32);
                stream>>*img;
                QString name;
                myStick.stickImgList.append(img);
                stream>>name;
                myStick.imgNameList.append(name);
            }
            myStick.stickImg = myStick.stickImgList[stickImgIdx];
        }
    }
    return stream;
}
// serializzatore per stickfigure
QDataStream & operator<< (QDataStream& stream, const StickFigure& myStickFigure){
    // per prima cosa registra il numero di stick che lo compongono

    stream<<myStickFigure.version;
    qDebug() << "before stream version";
    stream<<(int)myStickFigure.stickList.count();// beware, stream types must correspond! qsizetype != int
    stream<<myStickFigure.name;
    qDebug() << "before stream stick idx";
    // per ogni stick nella lista scrivi l'indice del genitore e poi serializza il tutto
    for(stick* s:myStickFigure.stickList){
        if(!s->isMaster()) //todo master replacement
        {
            qDebug() << "stk non master idx";
            s->parentIdx = (int)myStickFigure.stickList.indexOf(s->parent);
            qDebug() << "after streaming stick idx";
        }
        else{
            qDebug() << "stk  master idx";
            s->parentIdx = -1;
        }
        qDebug() << "before streaming stick";
        stream<<*s;
        qDebug() << "after streaming stick";
    }
    return stream;
}
// deserializza lo stickfigure
QDataStream & operator>> (QDataStream& stream,StickFigure& myStickFigure){

    int max = 0;
    stream>>myStickFigure.version;
    stream>>max; // quanti stick possiede?
    stream>>myStickFigure.name;
    myStickFigure.stickList.clear();
    // per ogni stick popola la lista sticklist
    for(int i = 0; i<max; i++){
        myStickFigure.currentStick = new stick(); // prepara il contenitore
        myStickFigure.stickList.append(myStickFigure.currentStick);
        stream>>*myStickFigure.currentStick; // "idrata il contenitore" con i dati nel file
        //myStickFigure.currentStick->Z = myStickFigure.baseZ+i*0.01;
        myStickFigure.currentStick->myStickFigure = &myStickFigure;
    }
    // ricostruisci le gerarchie di parentela tramite gli indici
    for(stick * s:myStickFigure.stickList){
        if(s->parentIdx>=0){
            // trova il genitore e aggiungi alla lista dei figli del genitore lo stick corrente
            s->parent = myStickFigure.stickList[s->parentIdx];
            stick * currentParent = s->parent;
            // procedi a ritroso per i genitori del genitore
            while(currentParent != nullptr){
                currentParent->children.append(s);
                if(currentParent->parentIdx>= 0 ){
                    currentParent = myStickFigure.stickList[currentParent->parentIdx];
                }
                else{ // arrivati allo stickmaster vai al prossimo
                    break;
                }
            }
        }
        else{ // se stiamo considerando lo stickmaster
            s->parent = nullptr;
            myStickFigure.masterStick = s;
            s->stickHandle[0]->setBrush(QBrush(Qt::green));
            s->stickHandle[0]->setZValue(101);
        }
        //if(s->isMaster()){ //todo master replacement

        //}
        if(!undoFlag && !loadingAnimationFlag && !libFlag && !copyFlag){
            myStickFigure.scene->addItem(s);
        }
        //s->refresh(0); // commentato per bug che non ho capito era necessario?
    }
    if(myStickFigure.stickList.count()>0){
        myStickFigure.currentStick = myStickFigure.stickList[0];
    }
    if(!undoFlag && !copyFlag){
        myStickFigure.updateIcon();
    }
    return stream;
}
// salva sul percorso impostato i dati serializzati dello stickfigure usando il datastream
void StickFigure::saveStickFigure(QString name)
{

    QFile file(name);
    if(!file.open(QIODevice::WriteOnly))
        return;
    QDataStream out(&file);
    out << *this;
    file.close();
}
void StickFigure::updateBoundingRects(){
    for(stick*s:stickList){
        s->updateBr(0);
    }
}
void cloneStickFigure(StickFigure* dest, StickFigure*src){

    // byte array stores serialized data
    QByteArray* byteArray = new QByteArray();
    // buffer temporarily holds serialized data
    QBuffer buffer1(byteArray);
    // use this buffer to store data from the object
    buffer1.open(QIODevice::WriteOnly);
    QDataStream myStream(&buffer1);
    myStream<<*(src);
    // now create a seconds buffer from which to read data of the bytearray
    QBuffer buffer2(byteArray);
    buffer2.open(QIODevice::ReadOnly);
    // a new data stream to deserialize
    QDataStream myStream2(&buffer2);
    // hydrate new frame with previous frame data
    myStream2>>*dest;


    if(!copyFlag)
    {
        dest->updateIcon();
    }
    delete byteArray;
}
//this function join parent stick with its children
QList<stick*> StickFigure::weld(stick* master){
    QList<stick*> toWeld;
    if(stickList.count()<= 1)
    {
        return toWeld;
    }
    //list of items to be deleted

    //populate the list with the immediate children of the selected stick
    for(stick* s:master->children)
    {
        if(s->parent == master && !(s->stepchild && master->isMaster()) ){ //todo master replacement
            toWeld.append(s);
        }
    }
    if(toWeld.isEmpty())
    {
        return toWeld;
    }
    // the master stick will now extend to the old extent of the welded element
    QPointF newP2(toWeld[0]->myLine.p2());
    master->myLine.setP2(newP2);
    if(master->stickType == stick::TAPER || master->stickType == stick::RECT || master->stickType == stick::TRAPEZOID)
    {
        master->imgAngle= -atan2(master->myLine.dx(),master->myLine.dy())*180/M_PI;
    }
    for(stick*s: toWeld)
    {
        //remove parentage of the welded sticks and put the new master in place
        for(stick* c:s->children)
        {
            if(c->parent == s)
            {
                c->myLine.setP1(newP2); //also update children p1 position
                if(c->stickType == stick::TAPER || c->stickType == stick::RECT || c->stickType == stick::TRAPEZOID)
                {
                    c->imgAngle= -atan2(c->myLine.dx(),c->myLine.dy())*180/M_PI;
                }
                c->parent = master;
            }
        }
        stick* recursive = master;
        while(recursive!=nullptr)
        { //remove the deleted elements from the list of children of the hierarchy
            recursive->children.removeAll(s);
            recursive = recursive->parent;
        }
    }//delete and remove from scenevivi la vitas
    for(stick*s: toWeld)
    {
        stickList.removeAll(s);
        scene->removeItem(s);
        delete s;
    }
    refresh(0);
    return toWeld;
}
// splitta uno stick in due parti, quella originale h il ruolo di genitore e quella splittata si inserisce nella catena dei figli
stick*  StickFigure::chop(stick* origin){
    // l'end point era quello dell'originale e lo starting point è il centro dell'originale
    QPointF oldP2 = origin->myLine.p2();
    QPointF newP2(0.5*(origin->myLine.p2().x()+origin->myLine.p1().x()),
                 0.5*(origin->myLine.p2().y()+origin->myLine.p1().y() ));
    origin->myLine.setP2(newP2);
    stick* toAdd = new stick();
    cloneStick(toAdd,origin);
    if(origin->isMaster()){ //todo master replacement
        //toAdd->master = false;
    }
    toAdd->myLine = QLineF(newP2,oldP2);
    toAdd->parent = origin;
    // nel caso sia un master considera solo i figli diretti e non gli stepchild
    if(origin->isMaster()){ //todo master replacement
        for(stick*s: stickList){
            if(!s->stepchild && s!=origin){
                toAdd->children.append(s);
                if(s->parent == origin){
                    s->parent = toAdd;
                }
            }
        }
    }
    else{
        toAdd->children = origin->children;
        for(stick*s:origin->children)
           {
            if(s->parent==origin)
                s->parent = toAdd;
        }
    }
    // inserisci lo stick aggiunto come children lungo la catena genitoriale
    stick* recoursive = origin;
    while(recoursive != nullptr){
        recoursive->children.append(toAdd);
        recoursive = recoursive->parent;
    }
    scene->addItem(toAdd);
    stickList.append(toAdd);

    refresh(0);
    return toAdd;
}

// attraverso il percorso file, carica un data stream su cui caricare i dati salvati
// chiama il deserializzatore per stickfigures per interpretare lo stream
void StickFigure::loadStickFigure(QString name)
{

    QFile file(name);
    if(!file.open(QIODevice::ReadOnly))
        return;
    QDataStream out(&file);
    out >> *this;

    file.close();
    qDebug("load finished");

}
// rotates the whole stickfigure
extern bool startStickFigureRotation;
void StickFigure::rotateStickFigure(QPointF *coord, QList<stick*> sticks, bool completeRotation){
    // traslation-only
    if(currentStick == masterStick
            &&QLineF(*coord,masterStick->myLine.p1()).length()<QLineF(*coord,masterStick->myLine.p2()).length()){
       traslate(-masterStick->myLine.p1().x()+coord->x(),-masterStick->myLine.p1().y()+coord->y());
       return;
    }
    // this records the angle between the mouse when it is first pressed and the master node of the stickfigure
    static float startingAngle = 0;
    //step 1 calcola l'angolo fra la posizione del mouse e il master node dello stickfigure
    QLineF connect;
    if(completeRotation)
        connect = QLineF(masterStick->myLine.p1(),*coord); //crea la linea connettendo i punti
    else
        connect = QLineF(currentStick->myLine.p1(),*coord); //crea la linea connettendo i punti
    float angle = connect.angle(); //dicci l'angolo
    // se la rotazione è appena iniziata sbbass ail flag e registra lo starting angle in modo che il primo valore inviato sia 0
    if(startStickFigureRotation){
        startStickFigureRotation = false;
        startingAngle = angle;
    }
    // sottrai l'angolo di inizio, la prima iterazione da sempre 0 ovviamente
    angle -= startingAngle;
    for(stick *s:sticks){

        s->rotate(angle);
    }

}
extern bool startStickFigureScale;
void StickFigure::scale(QPointF *coord){
    float scale = 0;
    static float startingDist = 0;
    static QPointF startingPoint;
    if(startStickFigureScale){

        startingPoint = *coord;
        startingDist = QLineF(startingPoint,masterStick->myLine.p1()).length();
        if(startingDist<=1)
            return;
        startStickFigureScale = false;
    }
    //step 1 calcola la distanza tra la coordinata iniziale e quella corrente, se si allontana dal master stick ingrandisci, senno rimpicicolisci
    QLineF connect(masterStick->myLine.p1(),*coord); //crea la linea connettendo i punti
    float actualDistance = connect.length();
    //float distanceDifference = actualDistance-startingDist;
    scale = actualDistance/startingDist;
    for(stick* s:stickList){
        s->scale(scale);
    }
    qDebug("scale = %f \r\n",scale);
}
void StickFigure::traslate(qreal dx, qreal dy){
    for(stick*s:stickList){
        s->myLine.translate(dx,dy);
        s->refresh(1);
    }
}
void mergeStickFigures(StickFigure* mainStickFigure, stick* mainStick,StickFigure* toJoin){
    QList<stick*> matchIndexes;
    // first add an invisible branch that links the two
    QLineF myLine(mainStick->myLine.p2(),toJoin->masterStick->myLine.p1());
    stick* linker = new stick(&myLine);

    mainStickFigure->stickList.append(linker);
    linker->parent = mainStick;

    linker->stickType = stick::LINE;
    linker->Pen= Qt::NoPen;
    mainStickFigure->scene->addItem(linker);
    //linker->setVisible(false);
    if(mainStick->stepchild)
        linker->stepchild = true;

    for(stick* s: toJoin->stickList){
        stick* toAdd = new stick(s);
        mainStickFigure->stickList.append(toAdd);
        //toAdd->master = false; //todo master replacement
        toAdd->stepchild = false;
        matchIndexes.append(toAdd);
        mainStickFigure->scene->addItem(toAdd);

        toAdd->refresh(0);
    }
    int i = 0;
    for(stick* s: toJoin->stickList){

        stick* toAdd = matchIndexes[i];
        if(mainStick->stepchild)
            toAdd->stepchild = true;
        if(s->isMaster()){ //todo master replacement
            //toAdd->master = false;
            toAdd->parent = linker;

        }
        else if(s->stepchild && s->parent->isMaster()) //todo master replacement
        {

            toAdd->parent = linker;
        }
        else{
            toAdd->parent = matchIndexes[toJoin->stickList.indexOf(s->parent)];
        }
        for(stick* c: s->children){

                toAdd->children.append(matchIndexes[toJoin->stickList.indexOf(c)]);
        }
        i++;
    }
    linker->children.append(matchIndexes[toJoin->stickList.indexOf(toJoin->masterStick)]->children);
    linker->children.append(matchIndexes[toJoin->stickList.indexOf(toJoin->masterStick)]);

    stick * recursive;
    recursive = mainStick;
    while(1){
        if(recursive == nullptr)
            break;
        recursive->children.append(matchIndexes[toJoin->stickList.indexOf(toJoin->masterStick)]->children);
        recursive->children.append(matchIndexes[toJoin->stickList.indexOf(toJoin->masterStick)]);
        recursive->children.append(linker);
        recursive = recursive->parent;
    }
    QList<stick*> stepChildToRemove;
    for(stick* child:toJoin->masterStick->children){
        if(child->stepchild)
            stepChildToRemove.append(matchIndexes[toJoin->stickList.indexOf(child)]);

    }
    for(stick* child:stepChildToRemove){
         matchIndexes[toJoin->stickList.indexOf(toJoin->masterStick)]->children.removeAll(child);
    }

}
// i genitori dello stick sono stepchild e invertono direzione fino al vecchio
// master stick che mantiene la stessa direzione
// la catena diretta diviene stepchild, vecchio master incluso
// mantiene le parentele e la direzione
// gli stepchild fratelli  rimangono stechild ma non cambiano direzione e mantengono la gerarchia
// gli stepchild zii diventano, per via dell'inversione
// diventano figli del figlio del vecchio genitore, per il resto mantengono la gerarchia
// i figli del nuovo master stick mantengono la gerarchia
// gli stick possono suddividersi in
//      Gerarchia sugli stick della catena che collega nuovo e vecchio master, da invertire in direzione, eccetto il vecchio master
//      Gerarchia sugli stick degli zii, che originano dalla catena genitoriale ma non ne fanno parte
//      Gerarchia sugli stick dei fratelli, che originano nello stesso punto del prossimo master
//      Figli del prossimo master che cambieranno il loro status da stepchild a non stepchild
void StickFigure::setStepChildAsMaster(stick* toMaster){
    QList<stick*> zii;
    QList<stick*> catenaGenitori;
    QList<stick*> fratelli;
    QList<stick*> oldStepChild;
    QList<stick*> oldDirectChain;
    QList<stick*> newDirectChain;
    stick* oldMaster = masterStick;
    stick* oldParent = toMaster->parent;
    QPointF p1Buf;
    QPointF p2Buf;

    // determina la catena genitoriale del nuovo masterstick
    newDirectChain = toMaster->children;
    for(stick* s: stickList){
        // se condivide lo stesso genitore, non è lo stesso stick da fare master e condivideva lo stesso status (stepchild o non stepchild) è fratello
        if(s->parent == toMaster->parent && s!=toMaster && s->stepchild == toMaster->stepchild){
                fratelli.append(s);
        }// se invece  contiene fra i suoi figli il nuovo master è nella catena genitoriale che dovrà essere invertita
        if(s->children.contains(toMaster)){
            catenaGenitori.append(s);
        }
        if(s->stepchild){
            oldStepChild.append(s);
        }
        else{ // tutti i non stepchild di prima
            oldDirectChain.append(s);
        }
    }
    // gli zii, sono gli stick che emergono dalla vecchia catena genitoriale (figli di uno di essi)
    for(stick* s: stickList){
        if(catenaGenitori.contains(s->parent) && // il genitore di questo stick appartiene alla catena genitoriale vecchia
                s!=toMaster && // non deve essere lo stick che renderemo amster
                 oldStepChild.contains(s) && // deve condividere lo stesso status di stepchild dello stick da rendere master
                !fratelli.contains(s)&& // non deve essere già stato messo nella lista dei fratelli
                !catenaGenitori.contains(s)){ // non deve far parte della catena genitoriale, deve essere un figlio di essa
            zii.append(s);
        }
    }
    // forma le catene dei fratelli, ovvero la gerarchia rimane la stessa, importala nel nuovo master
    for(stick* s:fratelli){
        s->parent = toMaster;
        toMaster->children.append(s);
        toMaster->children.append(s->children);
    }
    // inverti i genitori, prima mettili in una lista ordinata,
    // dove l'ultimo item sarà il vecchio genitore dello stick da e master
    // e il primo item sarà il vecchio master
    QList<stick*> chainToInvert;
    stick* recursive = toMaster->parent;
    for(stick*s:catenaGenitori){
       chainToInvert.insert(0,recursive);
       if(recursive!= oldMaster)
        recursive = recursive->parent;
    }// ora dal genitore del prossimo master al primo figlio del vecchio master riassegna il parent
    for(stick*s:chainToInvert){
       if(chainToInvert.indexOf(s)+1 == chainToInvert.length())
           break; // fermati al master
       s->parent = chainToInvert[chainToInvert.indexOf(s)+1]; // quello che era prima era il figlio diretto diviene il nuovo genitore
    }
    // il genitore del genitore del nuovo master, sarà il nuovo master stesso
    toMaster->parent->parent = toMaster;
    toMaster->parent = nullptr; // il nuovo master non ha genitori
    // pulisci la gerarchia dellacatena dei genitori da invertire
    for(stick* s: catenaGenitori){
        s->children.clear();
    }
    // nella catena da invertire scambia p1 con p2 e ricostruisci la lista dei figli
    for(stick* s:catenaGenitori){
        if(s!=oldMaster){ // il vecchio master mantiene la stessa direzione di prima
            p2Buf = s->myLine.p2();
            s->myLine.setP2(s->myLine.p1());
            s->myLine.setP1(p2Buf);
            if(s->stickType == stick::CIRCLE||s->stickType == stick::RECT || s->stickType == stick::TAPER || s->stickType == stick::TRAPEZOID){
                if(s->stickType != stick::CIRCLE && s->stickType != stick::IMAGE ){
                    float buf = s->imgHScale;
                    s->imgHScale = s->imgWScale;
                    s->imgWScale= buf;
                }

                s->imgAngle = s->imgAngle-180;
            }
        }
        stick* recursive = s->parent; // segna lo stick corrente come figlio di tutti i suoi genitori
        int n = 0;
        while(recursive != nullptr && recursive != oldMaster){
            recursive->children.append(s);
            recursive = recursive->parent;
            n++;
            if(n>stickList.count()){
                qDebug("error while inverting");
                return;
            }
        }
    }
    // importa il vecchio albero genialogico diretto fra i figli della catena genitoriale del nuovo master inverita e aggiornata
    oldDirectChain.removeAll(oldMaster);
    for(stick* s:catenaGenitori){
        s->children.append(oldDirectChain);
        //toMaster->children.append(s);
    }
    // al vecchio master imposta come figli solo quelli che prima erano il suo albero genialogico diretto (non stepchild)
    oldMaster->children.clear();
    oldMaster->children = oldDirectChain;
    // importa il vecchio albero genialogico diretto fra i figli del nuovo master
    toMaster->children.append(oldMaster->children);

    // aggiungi alla catena degli zii al ramo catena genitori invertita
    // ma ora che la catena è invertita, il genitore dello zio sarà quello che prima era il "nonno"
    for(stick* s:zii){
            s->parent = s->parent->parent;
            stick* recursive = s->parent;
            while(recursive){ // trasporta la nuova parentela a ritroso lungo la catena
                recursive->children.append(s);
                recursive->children.append(s->children);
                recursive = recursive->parent;

            }
    }
    // tutto è stepchild fuorchè i figli diretti del nuovo master
    for(stick*s:stickList){
        if(s==toMaster || newDirectChain.contains(s)){
            s->stepchild = false;
        }
        else{
            s->stepchild = true;
        }
    }
    //riassegna il ruolo del master
    masterStick = toMaster;
    //toMaster->master = true; //todo master replacement
    //oldMaster->master = false;

    refresh(0);
}
// rendi uno stick, che non era uno stepchild, nuovo master dello stickfigure
// gli stick possono raggrupparsi in:
// Fratelli del nuovo master che ne condividevano il genitore
// appartenenti alla catena che collega il nuovo master col vecchio, la cui direzione va invertita
// stick che nascevano nella catena sopracitata ma che generavano gerarchie parallele
// stick che prima erano stepchild, la cui gerarchia verrà unita con la catena genitoriale una volta invertita
void StickFigure::setDirectChainAsMaster(stick* toMaster){
    QList<stick*> zii;
    QList<stick*> catenaGenitori;
    QList<stick*> fratelli;
    QList<stick*> oldStepChild;
    stick* oldMaster = masterStick;
    QPointF p2Buf;
    for(stick* s: stickList){
        // la catena genitoriale sono quegli stick che collegano il prossimo master al vecchio master (incluso)
        if(s->children.contains(toMaster)){
            catenaGenitori.append(s);
        }
        // i fratelli sono gli stick che condividono la paternità col prossimo master
        else if(s->parent == toMaster->parent && s!=toMaster){
            if(!(s->parent == oldMaster && s->stepchild != toMaster->stepchild))
                fratelli.append(s);
        }
    }
    for(stick* s:stickList){
        if(s->stepchild){
            oldStepChild.append(s);
            s->stepchild = false;
        }
    }
    // gli zii, sono stick che derivano dalla catena genitoriale prima calcolata ma non ne fanno parte, non devono essere stepchild
    for(stick* s: stickList){
        if(catenaGenitori.contains(s->parent) &&
                s!=toMaster &&
                !catenaGenitori.contains(s) &&
                !fratelli.contains(s)&&
                !(oldStepChild.contains(s))){
            zii.append(s);
        }
    }
    // aggiorna lo status
    toMaster->stepchild = false;

    // la catena genitoriale va invertita, incluso il vecchio master si scambiano p1 con p2
    // e il primo figlio di ciascuno stick diventa il genitore di quello che prima era suo genitore
    QList<stick*> catenaDaInvertire;
    stick* iterator = toMaster->parent;
    for(stick*s: catenaGenitori) {
        catenaDaInvertire.insert(0,iterator);
        if(iterator != oldMaster)
            iterator = iterator->parent;
    }
    for(stick*s: catenaGenitori) {
        p2Buf = s->myLine.p2();
        s->myLine.setP2(s->myLine.p1());
        s->myLine.setP1(p2Buf);
        if( s->stickType == stick::CIRCLE||s->stickType == stick::RECT || s->stickType == stick::TAPER || s->stickType == stick::TRAPEZOID){
            s->imgAngle = s->imgAngle-180;
            if(s->stickType != stick::CIRCLE && s->stickType != stick::IMAGE ){
                float buf = s->imgHScale;
                s->imgHScale = s->imgWScale;
                s->imgWScale= buf;
            }

        }
        // pulisci i figli dello stick, la catena va ricostruita
        s->children.clear(); // attenzione il master così si perde anch gli stepchild che non vengono ricostruiti al for successivo
    }
    toMaster->parent->parent = toMaster;
    for(stick*s:catenaDaInvertire){
       if(catenaDaInvertire.indexOf(s)+1==catenaDaInvertire.length())
           break; // fermati al master
       s->parent = catenaDaInvertire[catenaDaInvertire.indexOf(s)+1]; // quello che era prima era il figlio diretto diviene il nuovo genitore
    }

    toMaster->parent = nullptr;
    // ricostruisci le liste dei figli ora che la catena è stata invertita
    for(stick*s: catenaGenitori) {
        stick * recursive = s->parent;
        s->stepchild = true;
        if(oldStepChild.contains(s))
                oldStepChild.removeAll(s);
        int n = 0;
        while(recursive){ // trasporta verso le gerarchie più alte, che puntano al nuovo master lo stick corrente come figlio
            recursive->children.append(s);
            recursive = recursive->parent;
            n++;
            if(n>stickList.count()){
                qDebug("error while inverting");
                return;
            }
        }
    }
    // i fratelli diventano stepchild ma la gerarchia non cambia
    for(stick* s:fratelli){
        if(oldStepChild.contains(s))
                oldStepChild.removeAll(s);
        s->stepchild = true;
        if(s != oldMaster){
            for(stick* c:s->children){
                if(oldStepChild.contains(s))
                        oldStepChild.removeAll(s);
                c->stepchild = true;

            }
            toMaster->children.append(s->children);
        }
        toMaster->children.append(s);
        s->parent = toMaster;

    }
    // gli zii ereditano la gerarchia, ma il loro pdre diviene quello che prima era il padre del padre per via dell'inversione
    // della catena genitoriale
    for(stick* s:zii){
        if(oldStepChild.contains(s))
                oldStepChild.removeAll(s);
        s->stepchild = true;
        stick* recursive;
        s->parent = s->parent->parent; // occhio conseguenza dell'inversione della catena genitoriale
        recursive = s->parent;
        while(recursive){ // trasporta la gerarchia verso il nuovo master lungo la catena
            recursive->children.append(s->children);
            recursive->children.append(s);
            recursive = recursive->parent;
        }
        for(stick* c:s->children){
            if(oldStepChild.contains(s))
                    oldStepChild.removeAll(s);
            c->stepchild = true;
        }
    }
    // quelli che prima erano stepchild, rimangono tali, ma con l'inversione della catena genitoriale,
    // la catena dei vecchi stepchild è contigua con la catena del vecchio master ora
    // trasporta la gerarchia a ritroso per quella strada verso il nuovo master partendo dal vecchio master
    for(stick*s:oldStepChild){
        s->stepchild = true;
        stick* recursive = oldMaster;
        while(recursive && !recursive->children.contains(s)){
            recursive->children.append(s);
            recursive = recursive->parent;
        }
    }
    toMaster->stepchild = false;
    masterStick = toMaster;
    //toMaster->master = true; //todo master replacement
    //oldMaster->master = false;
    refresh(0);
}
// questo algoritmo serve a cambiare l'estremità in cui vi è il punto di origine di un master con un estremo libero
//  - lo stick master non cambia, ma p1 e p2 si scambiano tra loro
//  - quelli che prima erano stepchild diventano catena diretta
//  - altrimenti se vi era soloc atena diretta, origine verso l'esterno, spostaimo l'origine verso l'interno e tutto diventa stecphildren
void StickFigure::invertMaster(stick* toMaster){
    QList<stick*> zii;
    QList<stick*> catenaGenitori;
    QList<stick*> fratelli;
    QList<stick*> oldStepChild;
    QPointF p2Buf;

    // inverti p1 con p2 per cambiare la direzione
    p2Buf = toMaster->myLine.p2();
    toMaster->myLine.setP2(toMaster->myLine.p1());
    toMaster->myLine.setP1(p2Buf);
    if(toMaster->stickType == stick::RECT ||toMaster->stickType == stick::CIRCLE|| toMaster->stickType == stick::TAPER || toMaster->stickType == stick::TRAPEZOID)
      {  toMaster->imgAngle = toMaster->imgAngle-180;
         if(toMaster->stickType != stick::CIRCLE && toMaster->stickType != stick::IMAGE ){
             float buf = toMaster->imgHScale;
             toMaster->imgHScale = toMaster->imgWScale;
             toMaster->imgWScale= buf;
         }

    }
    // qui si capisce quale inversione occorre compiere,
    // si parte con l'estremo di origine senza stepchild
    // o si parte con un estremo di origine da cui partono stepchild?
    QList<stick*> figli;
    QList<stick*> figliastri;
    for(stick*s:stickList){
        if(s->parent == masterStick && s->stepchild){
            figliastri.append(s);
        }
        else if(s->parent == masterStick && !s->stepchild)
            figli.append(s);
    }
    // se avevamo solo stepchildren, allora l'inversione consiste nel rendere tutto catena diretta e l'origine si sposta verso l' esterno
    if(figli.isEmpty()){
        for(stick*s: stickList){
            s->stepchild = false;
        }
    }
    else{ // altrimenti l'origine si sposta verso l'interno, le catene dirette diventano stepchildren
        for(stick*s: stickList){
            s->stepchild = true;
        }
        toMaster->stepchild = false;
    }
    // aggiorna lo status di master..ma non serve neanche
    masterStick = toMaster;
    //toMaster->master = true; //todo master replacement
    toMaster->stepchild = false;
    refresh(0);
}
// questo algoritmo semplificato rende master uno stick che correntemente è
// uno stepchild diretto del master attuale, in pratica
//  - La catena diretta dle master vecchio viene preservata, il nuovo master diventa genitore del master vecchio
//  - i fratelli del master vecchio acquisiscono come genitore il nuovo master e perdono lo status di stepchild
//  - il nuovo master acquisice stato di master, perde quello di stepchild,, il genitore diventa il nullptr
//      inoltre si invertono p1 e p2
void StickFigure::stepDownMaster(stick* toMaster){
    QList<stick*> zii;
    QList<stick*> catenaGenitori;
    QList<stick*> fratelli;
    QList<stick*> oldStepChild;
    QPointF p2Buf;

    stick* oldMaster = masterStick;

    for(stick*s: stickList){
        if(s->stepchild){
            oldMaster->children.removeAll(s);
        }
    }
    // tutti gli stick direttamente figli del vecchio master, se erano stepchild (quindi fratelli del vecchio master) cambiano parent
    // e perdono lo status id stepchild (anche i loro figli)
    for(stick* s: stickList){
        if(s->parent == oldMaster){
            if(s->stepchild && s!=toMaster){
                s->stepchild = false;
                for(stick* c:s->children){
                    c->stepchild = false;
                }
                s->parent = toMaster;
            }
        }
    }
    oldMaster->parent = toMaster;
    // inverti la direzione del nuovo master
    p2Buf = toMaster->myLine.p2();
    toMaster->myLine.setP2(toMaster->myLine.p1());
    toMaster->myLine.setP1(p2Buf);
    if(toMaster->stickType == stick::RECT ||toMaster->stickType == stick::CIRCLE||toMaster->stickType == stick::TAPER || toMaster->stickType == stick::TRAPEZOID)
        if(toMaster->stickType != stick::CIRCLE && toMaster->stickType != stick::IMAGE ){
            float buf = toMaster->imgHScale;
            toMaster->imgHScale = toMaster->imgWScale;
            toMaster->imgWScale= buf;
        }
    // il nuovo master ha come children tutti gli stick tranne se stesso
    toMaster->children.clear();
    toMaster->children = stickList;
    toMaster->children.removeAll(toMaster);

    masterStick = toMaster;
    //oldMaster->master = false;
//    toMaster->master = true; //todo master replacement
    toMaster->stepchild = false;
    toMaster->parent = nullptr;

    refresh(0);
}
// sets new endpoint of the stick and traslates all its children
void StickFigure::elongate(QPointF newEndPoint, stick* myStick){
    if(!stickList.contains(myStick))
        return;
    float dx = newEndPoint.x()-myStick->myLine.p2().x();
    float dy = newEndPoint.y()-myStick->myLine.p2().y();
    myStick->myLine.setP2(newEndPoint);
    for(stick*s: myStick->children){
        if(!(myStick->isMaster() && s->stepchild)){ //todo master replacement
            s->myLine.translate(dx,dy);

        }
    }
    if(myStick->stickType == stick::IMAGE || myStick->stickType  == stick::RECT/* || myStick->stickType  == stick::CIRCLE*/
            || myStick->stickType  == stick::TRAPEZOID || myStick->stickType  == stick::TAPER){
        myStick->imgAngle= -atan2(myStick->myLine.dx(),myStick->myLine.dy())*180/M_PI;
    }
    refresh(0);

}
// seleziona quale algoritmo usare a seconda dello stick che deve diventare master
void StickFigure::setMaster(stick* toMaster){
    QList<stick*> figli;
    QList<stick*> figliastri;
    masterStick->stickHandle[0]->setBrush(QBrush(Qt::red));
    masterStick->stickHandle[0]->setZValue(100);
    //prima, considerando il master, vedi quanti sick nascono dal suo punto di origine (figliastri) e quanti nascono dal suo estremo libero (figli)
    for(stick*s:stickList){
        if(s->parent == masterStick && s->stepchild){
            figliastri.append(s);
        }
        else if(s->parent == masterStick && !s->stepchild)
            figli.append(s);
    }
    // se siamo nella condizione di avere un estremo senza appendici e lo stick è già master, effettua una inversione
    if( toMaster == masterStick && (figli.isEmpty() || figliastri.isEmpty())){
        invertMaster(toMaster);
    }
    // se lo stick era uno stepchild che nasce direttamente dall'origine, allora sposta tutta la gerarchia un passo verso il basso
    else if(figliastri.contains(toMaster)){
        stepDownMaster(toMaster);
    }
    else if(toMaster == masterStick)
        return;
    else if(toMaster->stepchild) // master scelto fa parte dei non stepchild
        setStepChildAsMaster(toMaster);
    else
        setDirectChainAsMaster(toMaster); // masterscelto fa parte dei vecchi stepchild
    masterStick->stickHandle[0]->setBrush(QBrush(Qt::green));
    masterStick->stickHandle[0]->setZValue(101);
}
void splitStickFigures(StickFigure* split, stick* origin,StickFigure* branch){
    // crea una lista parallela con gli stick corrispondenti nella stickfigure originale
   QList<stick*> matchingIndex;
   // clonare tutti gli stick figli dello stick selezionato
    matchingIndex.append(origin);
    stick* toAdd = new stick(origin);
    branch->stickList.append(toAdd);        

    for(stick* s: origin->children)
    {
        if(s->parent != origin->parent)
        {
            matchingIndex.append(s);
            toAdd = new stick(s);
            branch->stickList.append(toAdd);
        }
    }

    //branch->stickList[matchingIndex.indexOf(origin)]->master = true; //todo master replacement
    branch->masterStick = branch->stickList[matchingIndex.indexOf(origin)];

    int i = 0;
    // in un altro ciclo for, per ogni stick clonato aggiungere gli stessi parent e gli stessi children sfruttando l'indicizzazione
    for(stick* s: branch->stickList){
        if(matchingIndex[i]->parent != origin->parent)
            s->parent = branch->stickList[matchingIndex.indexOf(matchingIndex[i]->parent)];
        for(stick*c: matchingIndex[i]->children){
            s->children.append(branch->stickList[matchingIndex.indexOf(c)]);
        }
        i++;
        split->scene->addItem(s);
        s->refresh(0);
    }

    split->deleteStick( split->stickList.indexOf(origin));
}
float StickFigure::minZ(){
    if(stickList.isEmpty()){
        return 0;
    }
    float minVal = stickList[0]->Z;
    for(stick*s:stickList){
        if(s->Z<minVal)
            minVal = s->Z;
    }
    return minVal;
}
float StickFigure::maxZ(){
    if(stickList.isEmpty()){
        return 0;
    }
    float maxVal = stickList[0]->Z;
    for(stick*s:stickList){
        if(s->Z>maxVal)
            maxVal = s->Z;
    }
    return maxVal;
}
void cloneStick(stick* dest, stick*src){
    // byte array stores serialized data
    QByteArray* byteArray = new QByteArray();
    // buffer temporarily holds serialized data
    QBuffer buffer1(byteArray);
    // use this buffer to store data from the object
    buffer1.open(QIODevice::WriteOnly);
    QDataStream myStream(&buffer1);
    myStream<<*(src);
    // now create a seconds buffer from which to read data of the bytearray
    QBuffer buffer2(byteArray);
    buffer2.open(QIODevice::ReadOnly);
    // a new data stream to deserialize
    QDataStream myStream2(&buffer2);
    // hydrate new frame with previous frame data
    myStream2>>*dest;
    delete byteArray;
}
QDataStream & operator>> (QDataStream& stream, stick& myStick);
